import cv2
import numpy as np


def decodificar_imagen(contents: bytes) -> np.ndarray:
    nparr = np.frombuffer(contents, np.uint8)
    imagen = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

    if imagen is None:
        raise ValueError("No se pudo decodificar la imagen.")

    return imagen


def preparar_imagen_modelo(imagen_cv2: np.ndarray) -> np.ndarray:
    if imagen_cv2 is None or imagen_cv2.size == 0:
        raise ValueError("La imagen recibida no es válida.")

    imagen_rgb = cv2.cvtColor(
        imagen_cv2,
        cv2.COLOR_BGR2RGB,
    )

    imagen_redimensionada = cv2.resize(
        imagen_rgb,
        (380, 380),
        interpolation=cv2.INTER_AREA,
    )

    imagen_array = imagen_redimensionada.astype(
        np.float32,
    )

    return np.expand_dims(
        imagen_array,
        axis=0,
    )


def obtener_coordenadas_hoja(imagen_cv2: np.ndarray) -> dict | None:
    try:
        alto_imagen, ancho_imagen = imagen_cv2.shape[:2]

        gris = cv2.cvtColor(imagen_cv2, cv2.COLOR_BGR2GRAY)

        suavizado = cv2.GaussianBlur(gris, (5, 5), 0)

        bordes = cv2.Canny(suavizado, 50, 150)

        contornos, _ = cv2.findContours(
            bordes,
            cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_SIMPLE,
        )

        if not contornos:
            return None

        contorno_mas_grande = max(
            contornos,
            key=cv2.contourArea,
        )

        area = cv2.contourArea(contorno_mas_grande)

        porcentaje_area = area / (ancho_imagen * alto_imagen)

        if porcentaje_area < 0.01:
            return None

        if porcentaje_area > 0.95:
            return None

        x, y, w, h = cv2.boundingRect(contorno_mas_grande)

        return {
            "x": int(x),
            "y": int(y),
            "w": int(w),
            "h": int(h),
            "image_width": int(ancho_imagen),
            "image_height": int(alto_imagen),
        }

    except cv2.error as e:
        print(f"Error de OpenCV al detectar la hoja: {e}")
        return None