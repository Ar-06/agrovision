# Agrovisión Rover

Agrovisión Rover es un sistema de monitoreo agrícola para hojas de palta. Integra una cámara ESP32-CAM, un rover controlado por ESP8266, un backend con FastAPI y TensorFlow, y un frontend en React para visualizar el stream de video, ejecutar diagnósticos automáticos y mostrar los resultados en tiempo real.

El objetivo del proyecto es capturar imágenes de hojas, enviarlas a un modelo de IA entrenado para clasificar enfermedades y presentar el diagnóstico con su nivel de confianza en un panel web.

## Arquitectura General

El sistema se divide en tres partes principales:

- `frontend`: aplicación web React que muestra el panel de control, consume el stream de la ESP32-CAM, captura imágenes periódicamente y visualiza diagnósticos.
- `agrovision-backend`: API FastAPI que recibe imágenes, procesa la imagen, ejecuta inferencia con TensorFlow, guarda detecciones en PostgreSQL y notifica eventos por WebSocket.
- `codigoESP`: sketches Arduino para controlar la ESP32-CAM y el ESP8266 del rover.

Flujo principal:

1. La ESP32-CAM expone un stream MJPEG y un endpoint `/capture` para fotografías JPEG.
2. El frontend muestra el stream configurado en `VITE_ESP32_STREAM`.
3. Cada 5 segundos el frontend solicita una imagen a `VITE_ESP32_CAPTURE`.
4. El frontend envía esa imagen al backend en `POST /predict`.
5. El backend decodifica la imagen, estima la zona de la hoja, prepara el tensor y ejecuta el modelo EfficientNetB4.
6. El backend guarda el resultado en la tabla `detections` de PostgreSQL.
7. El backend transmite la nueva detección por WebSocket en `/ws/detecciones`.
8. El frontend actualiza el diagnóstico, la confianza y el recuadro de detección sobre el video.
9. El ESP8266 controla motores DC y un motor NEMA 17 mediante Blynk.

## Estructura Del Proyecto

```text
agrovision/
├── README.md
├── codigoESP/
│   ├── esp32-cam.ino
│   └── esp8266.ino
├── agrovision-backend/
│   ├── app/
│   │   ├── api/
│   │   ├── database/
│   │   ├── models/
│   │   ├── services/
│   │   ├── websocket/
│   │   └── main.py
│   ├── migrations/
│   ├── models/
│   ├── requirements.txt
│   ├── .env.example
│   └── .python-version
└── frontend/
    ├── public/
    ├── src/
    │   ├── assets/
    │   ├── components/
    │   ├── hooks/
    │   ├── lib/
    │   ├── pages/
    │   ├── App.tsx
    │   ├── main.tsx
    │   └── index.css
    ├── package.json
    ├── vite.config.ts
    ├── components.json
    └── .env.example
```

## Frontend

El frontend se encuentra en `frontend/`. Es una aplicación React 19 con TypeScript, Vite, Tailwind CSS 4, shadcn/ui, Radix UI y lucide-react.

### Carpetas Y Archivos Del Frontend

- `frontend/src/main.tsx`: punto de entrada de React. Monta `<App />` dentro del elemento `#root` usando `StrictMode`.
- `frontend/src/App.tsx`: componente raíz de la aplicación. Renderiza el dashboard principal.
- `frontend/src/pages/Dashboard.tsx`: pantalla principal del sistema. Muestra el nombre del proyecto, el estado de conexión WebSocket, el stream de la ESP32-CAM, el recuadro de hoja detectada, el diagnóstico actual y el porcentaje de confianza.
- `frontend/src/hooks/useScanner.ts`: hook encargado del ciclo de captura y predicción. Solicita una foto al endpoint `/capture` de la ESP32-CAM, valida que sea JPEG, arma un `FormData` y lo envía al backend en `/predict`. También maneja errores de conexión, timeouts y evita solicitudes simultáneas.
- `frontend/src/hooks/useAgrovisionSocket.ts`: hook para conectarse al WebSocket del backend. Escucha mensajes `nueva_deteccion`, actualiza la última detección y reconecta automáticamente cada 3 segundos si se pierde la conexión.
- `frontend/src/components/ui/`: componentes de interfaz reutilizables generados o adaptados desde shadcn/ui.
- `frontend/src/components/ui/badge.tsx`: etiqueta visual para estados, como sistema en línea o patógeno detectado.
- `frontend/src/components/ui/card.tsx`: contenedores visuales usados para organizar el panel de cámara y análisis.
- `frontend/src/components/ui/progress.tsx`: barra de progreso usada para representar la confianza del modelo.
- `frontend/src/components/ui/button.tsx`: componente de botón reutilizable. Actualmente no es usado directamente por el dashboard, pero forma parte del set UI disponible.
- `frontend/src/lib/utils.ts`: función `cn()` para combinar clases CSS con `clsx` y resolver conflictos de Tailwind con `tailwind-merge`.
- `frontend/src/index.css`: estilos globales. Importa Tailwind CSS, animaciones, shadcn, fuente Geist y define tokens de tema.
- `frontend/src/assets/`: recursos estáticos usados por la aplicación.
- `frontend/public/`: archivos públicos servidos por Vite, como `favicon.svg` e `icons.svg`.
- `frontend/vite.config.ts`: configuración de Vite. Activa React, Tailwind, React Compiler mediante Babel y define el alias `@` hacia `src`.
- `frontend/components.json`: configuración de shadcn/ui, aliases y librería de iconos.
- `frontend/package.json`: scripts y dependencias del frontend.

### Variables De Entorno Del Frontend

Crear `frontend/.env` a partir de `frontend/.env.example`.

Variables usadas por el código:

```env
VITE_API_URL=http://localhost:8000
VITE_WS_URL=ws://localhost:8000/ws/detecciones
VITE_ESP32_STREAM=http://IP_DEL_ESP32:81/stream
VITE_ESP32_CAPTURE=http://IP_DEL_ESP32/capture
```

Notas:

- `VITE_API_URL` se concatena con `/predict` en `Dashboard.tsx`.
- `VITE_WS_URL` apunta al WebSocket del backend.
- `VITE_ESP32_STREAM` apunta al stream MJPEG de la ESP32-CAM.
- `VITE_ESP32_CAPTURE` apunta a la captura JPEG individual de la ESP32-CAM.
- El archivo `frontend/.env.example` actual contiene `VITE_ESP32_STREAM`, pero el código también requiere `VITE_ESP32_CAPTURE`.

### Comandos Del Frontend

Desde `frontend/`:

```bash
pnpm install
pnpm dev
pnpm build
pnpm lint
pnpm preview
```

## Backend

El backend se encuentra en `agrovision-backend/`. Es una API construida con FastAPI, SQLAlchemy async, PostgreSQL, OpenCV y TensorFlow/Keras.

### Carpetas Y Archivos Del Backend

- `agrovision-backend/app/main.py`: crea la aplicación FastAPI, configura CORS, registra rutas y usa un ciclo de vida para crear tablas al iniciar y cerrar la conexión a la base de datos al finalizar.
- `agrovision-backend/app/api/routes.py`: define los endpoints HTTP y WebSocket.
- `GET /`: endpoint de salud básico que confirma que el servidor está activo.
- `POST /predict`: recibe una imagen por formulario, la procesa, ejecuta inferencia, guarda la detección y transmite el resultado por WebSocket.
- `WS /ws/detecciones`: canal WebSocket para enviar detecciones en tiempo real al frontend. Responde a `ping` con un mensaje `pong`.
- `agrovision-backend/app/database/connection.py`: carga `DATABASE_URL` desde `.env`, crea el engine async de SQLAlchemy, define `SessionLocal`, la clase base declarativa y helpers para obtener/cerrar sesiones.
- `agrovision-backend/app/models/detection.py`: modelo SQLAlchemy `Detection`, asociado a la tabla `detections`.
- `agrovision-backend/app/services/image_processing.py`: funciones de procesamiento con OpenCV. Decodifica bytes JPEG, convierte imágenes a RGB, redimensiona a `380x380`, genera el batch para el modelo y calcula un bounding box aproximado de la hoja usando escala de grises, desenfoque, Canny y contornos.
- `agrovision-backend/app/services/inference.py`: carga el modelo `modelo_palta_efficientnetb4_production.h5`, define las clases disponibles y ejecuta la predicción.
- `agrovision-backend/app/websocket/manager.py`: administrador de conexiones WebSocket. Mantiene conexiones activas, acepta clientes, elimina desconectados y envía mensajes broadcast.
- `agrovision-backend/models/`: modelos entrenados en formato `.h5`.
- `modelo_palta_efficientnetb4_production.h5`: modelo usado por el backend en producción.
- `modelo_palta_salvaje_checkpoint.h5`: checkpoint adicional conservado en el repositorio.
- `agrovision-backend/migrations/remove_gps_columns.sql`: migración SQL para eliminar columnas GPS antiguas de `detections`.
- `agrovision-backend/requirements.txt`: dependencias Python fijadas.
- `agrovision-backend/.python-version`: versión Python objetivo, `3.10.11`.
- `agrovision-backend/install-pyenv-win.ps1`: script auxiliar para instalar/configurar pyenv-win.

### Modelo De Datos

Tabla `detections`:

- `id`: entero autoincremental, clave primaria.
- `diagnostico`: texto con la clase detectada.
- `confianza`: porcentaje de confianza de la predicción.
- `fecha_creacion`: fecha de creación generada por la base de datos.

### Clases Del Modelo De IA

El backend clasifica la imagen en una de estas clases:

- `Bacterias`
- `Bichos`
- `Isariopsis`
- `Oidio`
- `Otras_Especies`
- `Sanas`
- `Septoria`

### Variables De Entorno Del Backend

Crear `agrovision-backend/.env` a partir de `agrovision-backend/.env.example`.

```env
DATABASE_URL=postgresql+asyncpg://usuario:password@localhost:5432/agrovision
```

El backend falla al iniciar si `DATABASE_URL` no está definida.

### Ejecución Del Backend

Desde `agrovision-backend/`:

```bash
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --reload
```

Al iniciar, FastAPI ejecuta `create_tables()` para crear las tablas definidas por SQLAlchemy si aún no existen.

## Código ESP

La carpeta `codigoESP/` contiene los programas para los microcontroladores del rover.

### `codigoESP/esp32-cam.ino`

Sketch para una ESP32-CAM AI Thinker. Su responsabilidad es capturar imagen, publicar video en red local y exponer endpoints HTTP para que el frontend pueda consumir la cámara.

Funciones principales:

- Se conecta a Wi-Fi usando `ssid` y `password`.
- Inicializa la cámara con los pines del módulo AI Thinker.
- Configura el sensor en JPEG, normalmente `FRAMESIZE_VGA` si hay PSRAM.
- Ajusta brillo, contraste, saturación, nitidez, balance de blancos, exposición, ganancia y correcciones de imagen.
- Expone una página web de control en `http://IP_DEL_ESP32/`.
- Expone una captura JPEG individual en `http://IP_DEL_ESP32/capture`.
- Expone el stream MJPEG en `http://IP_DEL_ESP32:81/stream`.
- Expone estado del dispositivo en `http://IP_DEL_ESP32/status`.
- Expone controles de cámara en `http://IP_DEL_ESP32/control?var=VARIABLE&val=VALOR`.
- Controla la intensidad del flash LED en GPIO 4.
- Agrega cabeceras CORS para permitir consumo desde el frontend.
- Reintenta conexión Wi-Fi si se pierde la señal.

Endpoints de la ESP32-CAM:

- `/`: panel web embebido con botones para iniciar/detener stream, capturar fotografía y modificar parámetros.
- `/capture`: devuelve una imagen `image/jpeg` capturada en el momento.
- `/status`: devuelve JSON con IP, RSSI, PSRAM, brillo, contraste, saturación, calidad, tamaño de frame e intensidad LED.
- `/control`: modifica parámetros como `brightness`, `contrast`, `saturation`, `quality`, `ae_level` y `led_intensity`.
- `:81/stream`: stream MJPEG continuo.

Antes de cargar el sketch se deben configurar:

```cpp
const char* ssid = "NOMBRE_WIFI";
const char* password = "CONTRASEÑA_WIFI";
```

### `codigoESP/esp8266.ino`

Sketch para ESP8266 encargado del movimiento del rover y del control de altura mediante un motor NEMA 17.

Funciones principales:

- Se conecta a Wi-Fi y Blynk.
- Controla dos motores DC mediante un puente H.
- Controla un motor paso a paso NEMA 17 mediante driver A4988.
- Usa la librería `AccelStepper` para movimiento continuo y desaceleración controlada.
- Mantiene `Blynk.run()` y `nema.run()` en el `loop()` sin delays para no bloquear el control.

Pines usados para motores DC:

- `IN1 = GPIO5 / D1`
- `IN2 = GPIO13 / D7`
- `IN3 = GPIO0 / D3`
- `IN4 = GPIO2 / D4`

Pines usados para A4988 y NEMA 17:

- `STEP_PIN = GPIO4 / D2`
- `DIR_PIN = GPIO14 / D5`

Controles Blynk:

- `V1`: avanzar.
- `V2`: retroceder.
- `V3`: girar a la izquierda.
- `V4`: girar a la derecha.
- `V5`: subir NEMA 17.
- `V6`: bajar NEMA 17.

Antes de cargar el sketch se deben configurar:

```cpp
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

char ssid[] = "NOMBRE_WIFI";
char pass[] = "CONTRASEÑA_WIFI";
```

## Instalación Y Puesta En Marcha

### 1. Preparar PostgreSQL

Crear una base de datos para el proyecto y configurar `DATABASE_URL` en `agrovision-backend/.env`.

Ejemplo:

```env
DATABASE_URL=postgresql+asyncpg://postgres:postgres@localhost:5432/agrovision
```

### 2. Iniciar Backend

```bash
cd agrovision-backend
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --reload
```

API local por defecto:

```text
http://localhost:8000
```

### 3. Cargar Código En ESP32-CAM

Configurar Wi-Fi en `codigoESP/esp32-cam.ino`, cargar el sketch desde Arduino IDE o PlatformIO y revisar el monitor serial para obtener la IP.

URLs esperadas:

```text
http://IP_DEL_ESP32/
http://IP_DEL_ESP32/capture
http://IP_DEL_ESP32/status
http://IP_DEL_ESP32:81/stream
```

### 4. Cargar Código En ESP8266

Configurar Wi-Fi y credenciales Blynk en `codigoESP/esp8266.ino`, cargar el sketch y verificar desde la app o panel Blynk que los botones virtuales `V1` a `V6` estén configurados.

### 5. Iniciar Frontend

Crear `frontend/.env`:

```env
VITE_API_URL=http://localhost:8000
VITE_WS_URL=ws://localhost:8000/ws/detecciones
VITE_ESP32_STREAM=http://IP_DEL_ESP32:81/stream
VITE_ESP32_CAPTURE=http://IP_DEL_ESP32/capture
```

Ejecutar:

```bash
cd frontend
pnpm install
pnpm dev
```

Abrir la URL mostrada por Vite, normalmente:

```text
http://localhost:5173
```

## Endpoints Principales

Backend:

- `GET /`: estado básico del servidor.
- `POST /predict`: recibe `multipart/form-data` con el campo `file` y devuelve diagnóstico.
- `WS /ws/detecciones`: emite nuevas detecciones en tiempo real.

Respuesta esperada de `POST /predict`:

```json
{
  "id": 1,
  "diagnostico": "Sanas",
  "confianza": 97.4,
  "box": {
    "x": 120,
    "y": 80,
    "w": 260,
    "h": 300,
    "image_width": 640,
    "image_height": 480
  },
  "fecha_creacion": "2026-07-15T10:30:00"
}
```

ESP32-CAM:

- `GET /`: panel web de cámara.
- `GET /capture`: imagen JPEG.
- `GET /status`: estado JSON de cámara y red.
- `GET /control?var=brightness&val=1`: control de parámetros.
- `GET :81/stream`: stream MJPEG.

## Consideraciones Técnicas

- El modelo espera imágenes RGB redimensionadas a `380x380`.
- El backend carga el modelo al importar `app.services.inference`; si falla la carga, `/predict` responde error `503`.
- El frontend captura una imagen cada 5 segundos.
- `useScanner` aborta capturas o predicciones que exceden 15 segundos.
- El bounding box de la hoja es aproximado y se basa en contornos, no en un modelo de detección dedicado.
- CORS está abierto en backend y ESP32-CAM para facilitar pruebas locales.
- La ESP32-CAM usa dos servidores HTTP: puerto `80` para panel/captura/control/estado y puerto `81` para streaming.
- El ESP8266 depende de Blynk para recibir comandos de movimiento.

## Dependencias Principales

Frontend:

- React 19
- TypeScript
- Vite
- Tailwind CSS 4
- shadcn/ui
- Radix UI
- lucide-react

Backend:

- FastAPI
- Uvicorn
- SQLAlchemy async
- asyncpg
- PostgreSQL
- TensorFlow/Keras
- OpenCV headless
- NumPy
- Pillow

ESP:

- Arduino core para ESP32 y ESP8266
- `esp_camera`
- `esp_http_server`
- `ESP8266WiFi`
- `BlynkSimpleEsp8266`
- `AccelStepper`

## Mantenimiento

- Si cambia la IP de la ESP32-CAM, actualizar `VITE_ESP32_STREAM` y `VITE_ESP32_CAPTURE`.
- Si cambia el endpoint del backend, actualizar `VITE_API_URL` y `VITE_WS_URL`.
- Si se reemplaza el modelo de IA, actualizar `MODELO_PATH` en `agrovision-backend/app/services/inference.py` si el nombre del archivo cambia.
- Si se agregan nuevas clases al modelo, actualizar `CLASS_NAMES` en `agrovision-backend/app/services/inference.py` en el mismo orden de salida del modelo.
- Si se modifican los controles Blynk, actualizar los handlers `BLYNK_WRITE(Vx)` en `codigoESP/esp8266.ino` y el panel Blynk correspondiente.
