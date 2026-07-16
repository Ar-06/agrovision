from fastapi import APIRouter, Depends, File, Query, UploadFile, WebSocket
from sqlalchemy.ext.asyncio import AsyncSession
from fastapi.responses import JSONResponse
from fastapi.websockets import WebSocketDisconnect

from app.services.image_processing import (
    decodificar_imagen,
    obtener_coordenadas_hoja,
    preparar_imagen_modelo,
)
from app.services.inference import predecir_imagen
from app.services.detection_history import obtener_historial_detecciones
from app.websocket.manager import manager
from app.database.connection import get_db
from app.models.detection import Detection

router = APIRouter()

@router.get("/")
def home():
    return {
        "mensaje": (
            "Servidor Agrovisión activo y "
            "esperando imágenes del ESP32-CAM."
        )
    }

@router.websocket("/ws/detecciones")
async def websocket_detecciones(
    websocket: WebSocket,
):
    await manager.connect(websocket)

    try:
        while True:
            mensaje = await websocket.receive_text()

            if mensaje == "ping":
                await websocket.send_json({
                    "tipo": "pong",
                    "mensaje": "Servidor recibido.",
                })
    except WebSocketDisconnect:
        manager.disconnect(websocket)

@router.get("/detecciones")
async def listar_detecciones(
    limite: int = Query(default=20, ge=1, le=100),
    db: AsyncSession = Depends(get_db),
):
    return await obtener_historial_detecciones(db, limite)

@router.get("/detecciones/historial")
async def obtener_historial(
    limite: int = Query(default=20, ge=1, le=100),
    db: AsyncSession = Depends(get_db),
):
    return await obtener_historial_detecciones(db, limite)

@router.post("/predict")
async def predict_image(
    file: UploadFile = File(...),
    db: AsyncSession = Depends(get_db),
):
    try:
        contents = await file.read()

        imagen_cv2 = decodificar_imagen(contents)

        caja = obtener_coordenadas_hoja(imagen_cv2)

        img_array = preparar_imagen_modelo(imagen_cv2)

        clase_detectada, confianza = predecir_imagen(img_array)

        nueva_deteccion = Detection(
            diagnostico=clase_detectada,
            confianza=round(confianza, 1),
        )

        db.add(nueva_deteccion)

        await db.commit()
        await db.refresh(nueva_deteccion)

        datos = {
            "id": nueva_deteccion.id,
            "diagnostico": nueva_deteccion.diagnostico,
            "confianza": nueva_deteccion.confianza,
            "box": caja,
            "fecha_creacion": (
                nueva_deteccion.fecha_creacion.isoformat()
                if nueva_deteccion.fecha_creacion
                else None
            ),
        }

        await manager.broadcast(
            {
                "tipo": "nueva_deteccion",
                "datos": datos,
            }
        )

        return JSONResponse(content=datos)

    except ValueError as e:
        await db.rollback()

        return JSONResponse(
            status_code=400,
            content={"error": str(e)},
        )

    except RuntimeError as e:
        await db.rollback()

        return JSONResponse(
            status_code=503,
            content={"error": str(e)},
        )

    except Exception as e:
        await db.rollback()

        return JSONResponse(
            status_code=500,
            content={"error": str(e)},
        )
