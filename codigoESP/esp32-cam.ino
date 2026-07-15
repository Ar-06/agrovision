#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_timer.h"

// =====================================================
// CONFIGURACIÓN WIFI
// =====================================================

const char* ssid = "NOMBRE_WIFI";
const char* password = "CONTRASEÑA_WIFI";

// =====================================================
// PINES ESP32-CAM AI THINKER
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LED_GPIO_NUM       4

// =====================================================
// CONFIGURACIÓN DEL STREAM
// =====================================================

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

static const char* STREAM_BOUNDARY =
    "\r\n--" PART_BOUNDARY "\r\n";

static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n\r\n";

// Servidor web principal: puerto 80
httpd_handle_t cameraHttpd = nullptr;

// Servidor de streaming: puerto 81
httpd_handle_t streamHttpd = nullptr;

// Intensidad inicial del LED: 0 a 255
int ledIntensity = 25;

// =====================================================
// PÁGINA WEB
// =====================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta
    name="viewport"
    content="width=device-width, initial-scale=1.0"
  >

  <title>AgroVisión ESP32-CAM</title>

  <style>
    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      padding: 20px;
      background: #101712;
      color: #f5f5f5;
      font-family: Arial, sans-serif;
      text-align: center;
    }

    .container {
      max-width: 850px;
      margin: auto;
    }

    h1 {
      color: #68d391;
      margin-bottom: 5px;
    }

    .subtitle {
      color: #b7c5ba;
      margin-bottom: 20px;
    }

    .camera-container {
      width: 100%;
      background: #000;
      border: 2px solid #2f855a;
      border-radius: 12px;
      overflow: hidden;
    }

    #stream {
      display: block;
      width: 100%;
      min-height: 240px;
      object-fit: contain;
      background: #000;
    }

    .controls {
      margin-top: 18px;
      padding: 18px;
      background: #18241c;
      border-radius: 12px;
    }

    .button-group {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: 10px;
      margin-bottom: 18px;
    }

    button, a.button {
      border: none;
      border-radius: 8px;
      padding: 11px 18px;
      background: #2f855a;
      color: white;
      cursor: pointer;
      font-size: 15px;
      text-decoration: none;
    }

    button:hover, a.button:hover {
      background: #38a169;
    }

    .danger {
      background: #c53030;
    }

    .danger:hover {
      background: #e53e3e;
    }

    .slider-group {
      margin: 15px auto;
      max-width: 500px;
      text-align: left;
    }

    label {
      display: block;
      margin-bottom: 7px;
      font-weight: bold;
    }

    input[type="range"] {
      width: 100%;
    }

    .value {
      color: #68d391;
      font-weight: bold;
    }

    .status {
      margin-top: 14px;
      color: #b7c5ba;
      font-size: 14px;
    }
  </style>
</head>

<body>
  <div class="container">
    <h1>AgroVisión ESP32-CAM</h1>

    <p class="subtitle">
      Cámara para clasificación de enfermedades en hojas de palta
    </p>

    <div class="camera-container">
      <img id="stream" alt="Video ESP32-CAM">
    </div>

    <div class="controls">
      <div class="button-group">
        <button onclick="iniciarStream()">
          Iniciar stream
        </button>

        <button class="danger" onclick="detenerStream()">
          Detener stream
        </button>

        <button onclick="reiniciarStream()">
          Reiniciar stream
        </button>

        <a
          class="button"
          href="/capture"
          target="_blank"
        >
          Tomar fotografía
        </a>
      </div>

      <div class="slider-group">
        <label>
          Intensidad del flash:
          <span id="ledValue" class="value">25</span>
        </label>

        <input
          id="ledSlider"
          type="range"
          min="0"
          max="255"
          value="25"
          oninput="actualizarLed(this.value)"
        >
      </div>

      <div class="slider-group">
        <label>
          Brillo:
          <span id="brightnessValue" class="value">1</span>
        </label>

        <input
          type="range"
          min="-2"
          max="2"
          value="1"
          oninput="actualizarControl(
            'brightness',
            this.value,
            'brightnessValue'
          )"
        >
      </div>

      <div class="slider-group">
        <label>
          Nivel de exposición:
          <span id="exposureValue" class="value">1</span>
        </label>

        <input
          type="range"
          min="-2"
          max="2"
          value="1"
          oninput="actualizarControl(
            'ae_level',
            this.value,
            'exposureValue'
          )"
        >
      </div>

      <div class="slider-group">
        <label>
          Calidad JPEG:
          <span id="qualityValue" class="value">12</span>
        </label>

        <input
          type="range"
          min="8"
          max="25"
          value="12"
          oninput="actualizarControl(
            'quality',
            this.value,
            'qualityValue'
          )"
        >
      </div>

      <p class="status" id="status">
        Stream detenido
      </p>
    </div>
  </div>

  <script>
    const stream = document.getElementById("stream");
    const statusText = document.getElementById("status");

    const streamUrl =
      `${location.protocol}//${location.hostname}:81/stream`;

    function iniciarStream() {
      stream.src = streamUrl + "?t=" + Date.now();
      statusText.textContent = "Stream activo";
    }

    function detenerStream() {
      stream.removeAttribute("src");
      stream.src = "";
      statusText.textContent = "Stream detenido";
    }

    function reiniciarStream() {
      detenerStream();

      setTimeout(() => {
        iniciarStream();
      }, 300);
    }

    async function actualizarLed(value) {
      document.getElementById("ledValue").textContent = value;

      try {
        await fetch(`/control?var=led_intensity&val=${value}`);
      } catch (error) {
        console.error("Error actualizando LED:", error);
      }
    }

    async function actualizarControl(variable, value, elementId) {
      document.getElementById(elementId).textContent = value;

      try {
        await fetch(`/control?var=${variable}&val=${value}`);
      } catch (error) {
        console.error("Error actualizando cámara:", error);
      }
    }

    window.addEventListener("load", iniciarStream);
  </script>
</body>
</html>
)rawliteral";

// =====================================================
// MANEJO DEL LED
// =====================================================

void configurarLed() {
  ledcAttach(LED_GPIO_NUM, 5000, 8);
  ledcWrite(LED_GPIO_NUM, 0);
}

void encenderLed(int intensidad) {
  intensidad = constrain(intensidad, 0, 255);
  ledcWrite(LED_GPIO_NUM, intensidad);
}

void apagarLed() {
  ledcWrite(LED_GPIO_NUM, 0);
}

// =====================================================
// CABECERAS CORS
// =====================================================

void agregarCors(httpd_req_t* req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(
      req,
      "Access-Control-Allow-Methods",
      "GET, OPTIONS"
  );
  httpd_resp_set_hdr(
      req,
      "Access-Control-Allow-Headers",
      "Content-Type"
  );
}

// =====================================================
// PÁGINA PRINCIPAL
// =====================================================

static esp_err_t indexHandler(httpd_req_t* req) {
  agregarCors(req);

  httpd_resp_set_type(req, "text/html");

  return httpd_resp_send(
      req,
      INDEX_HTML,
      HTTPD_RESP_USE_STRLEN
  );
}

// =====================================================
// CAPTURA INDIVIDUAL
// =====================================================

static esp_err_t captureHandler(httpd_req_t* req) {
  camera_fb_t* frame = nullptr;

  // Encendemos el flash antes de capturar.
  if (ledIntensity > 0) {
    encenderLed(ledIntensity);
    delay(120);
  }

  frame = esp_camera_fb_get();

  apagarLed();

  if (!frame) {
    Serial.println("Error capturando imagen");

    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  agregarCors(req);

  httpd_resp_set_type(req, "image/jpeg");

  httpd_resp_set_hdr(
      req,
      "Content-Disposition",
      "inline; filename=agrovision_capture.jpg"
  );

  esp_err_t result = httpd_resp_send(
      req,
      reinterpret_cast<const char*>(frame->buf),
      frame->len
  );

  Serial.printf(
      "Captura enviada: %u bytes\n",
      static_cast<unsigned int>(frame->len)
  );

  esp_camera_fb_return(frame);

  return result;
}

// =====================================================
// STREAM MJPEG
// =====================================================

static esp_err_t streamHandler(httpd_req_t* req) {
  camera_fb_t* frame = nullptr;
  esp_err_t result = ESP_OK;

  char headerBuffer[128];

  result = httpd_resp_set_type(
      req,
      STREAM_CONTENT_TYPE
  );

  if (result != ESP_OK) {
    return result;
  }

  agregarCors(req);

  Serial.println("Cliente conectado al stream");

  while (true) {
    frame = esp_camera_fb_get();

    if (!frame) {
      Serial.println("Error obteniendo frame");
      result = ESP_FAIL;
      break;
    }

    result = httpd_resp_send_chunk(
        req,
        STREAM_BOUNDARY,
        strlen(STREAM_BOUNDARY)
    );

    if (result == ESP_OK) {
      size_t headerLength = snprintf(
          headerBuffer,
          sizeof(headerBuffer),
          STREAM_PART,
          frame->len
      );

      result = httpd_resp_send_chunk(
          req,
          headerBuffer,
          headerLength
      );
    }

    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(
          req,
          reinterpret_cast<const char*>(frame->buf),
          frame->len
      );
    }

    esp_camera_fb_return(frame);
    frame = nullptr;

    if (result != ESP_OK) {
      Serial.println("Cliente desconectado del stream");
      break;
    }

    // Permite que otras tareas del ESP32 se ejecuten.
    delay(1);
  }

  return result;
}

// =====================================================
// CONTROL DE CÁMARA
// =====================================================

static esp_err_t controlHandler(httpd_req_t* req) {
  char query[128];
  char variable[32];
  char valueText[32];

  size_t queryLength =
      httpd_req_get_url_query_len(req) + 1;

  if (queryLength <= 1 || queryLength > sizeof(query)) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  if (
      httpd_req_get_url_query_str(
          req,
          query,
          sizeof(query)
      ) != ESP_OK
  ) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  if (
      httpd_query_key_value(
          query,
          "var",
          variable,
          sizeof(variable)
      ) != ESP_OK ||
      httpd_query_key_value(
          query,
          "val",
          valueText,
          sizeof(valueText)
      ) != ESP_OK
  ) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  int value = atoi(valueText);

  sensor_t* sensor = esp_camera_sensor_get();

  if (sensor == nullptr) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  int result = 0;

  if (strcmp(variable, "brightness") == 0) {
    value = constrain(value, -2, 2);
    result = sensor->set_brightness(sensor, value);
  }
  else if (strcmp(variable, "contrast") == 0) {
    value = constrain(value, -2, 2);
    result = sensor->set_contrast(sensor, value);
  }
  else if (strcmp(variable, "saturation") == 0) {
    value = constrain(value, -2, 2);
    result = sensor->set_saturation(sensor, value);
  }
  else if (strcmp(variable, "quality") == 0) {
    value = constrain(value, 8, 63);
    result = sensor->set_quality(sensor, value);
  }
  else if (strcmp(variable, "ae_level") == 0) {
    value = constrain(value, -2, 2);
    result = sensor->set_ae_level(sensor, value);
  }
  else if (strcmp(variable, "led_intensity") == 0) {
    ledIntensity = constrain(value, 0, 255);

    // Solo actualizamos el valor.
    // El LED se enciende durante /capture.
    apagarLed();
  }
  else {
    Serial.printf(
        "Control desconocido: %s\n",
        variable
    );

    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  if (result != 0) {
    Serial.printf(
        "Error configurando %s con valor %d\n",
        variable,
        value
    );

    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  Serial.printf(
      "Configuración: %s = %d\n",
      variable,
      value
  );

  agregarCors(req);

  return httpd_resp_send(req, nullptr, 0);
}

// =====================================================
// INFORMACIÓN DEL DISPOSITIVO
// =====================================================

static esp_err_t statusHandler(httpd_req_t* req) {
  sensor_t* sensor = esp_camera_sensor_get();

  if (sensor == nullptr) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char json[512];

  snprintf(
      json,
      sizeof(json),
      "{"
        "\"ip\":\"%s\","
        "\"rssi\":%d,"
        "\"psram\":%s,"
        "\"brightness\":%d,"
        "\"contrast\":%d,"
        "\"saturation\":%d,"
        "\"quality\":%d,"
        "\"framesize\":%d,"
        "\"led_intensity\":%d"
      "}",
      WiFi.localIP().toString().c_str(),
      WiFi.RSSI(),
      psramFound() ? "true" : "false",
      sensor->status.brightness,
      sensor->status.contrast,
      sensor->status.saturation,
      sensor->status.quality,
      sensor->status.framesize,
      ledIntensity
  );

  agregarCors(req);
  httpd_resp_set_type(req, "application/json");

  return httpd_resp_send(
      req,
      json,
      HTTPD_RESP_USE_STRLEN
  );
}

// =====================================================
// INICIAR SERVIDORES
// =====================================================

void iniciarServidorCamara() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  config.max_uri_handlers = 10;
  config.stack_size = 8192;
  config.server_port = 80;
  config.ctrl_port = 32768;

  httpd_uri_t indexUri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = indexHandler,
    .user_ctx = nullptr
  };

  httpd_uri_t captureUri = {
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = captureHandler,
    .user_ctx = nullptr
  };

  httpd_uri_t controlUri = {
    .uri = "/control",
    .method = HTTP_GET,
    .handler = controlHandler,
    .user_ctx = nullptr
  };

  httpd_uri_t statusUri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = statusHandler,
    .user_ctx = nullptr
  };

  Serial.println("Iniciando servidor principal en puerto 80");

  if (httpd_start(&cameraHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(cameraHttpd, &indexUri);
    httpd_register_uri_handler(cameraHttpd, &captureUri);
    httpd_register_uri_handler(cameraHttpd, &controlUri);
    httpd_register_uri_handler(cameraHttpd, &statusUri);
  } else {
    Serial.println("No se pudo iniciar el servidor principal");
  }

  // Segundo servidor para el stream.
  config.server_port = 81;
  config.ctrl_port = 32769;

  httpd_uri_t streamUri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = streamHandler,
    .user_ctx = nullptr
  };

  Serial.println("Iniciando stream en puerto 81");

  if (httpd_start(&streamHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(streamHttpd, &streamUri);
  } else {
    Serial.println("No se pudo iniciar el servidor de stream");
  }
}

// =====================================================
// CONFIGURACIÓN DEL SENSOR
// =====================================================

bool configurarCamara() {
  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIXFORMAT_JPEG;

  /*
   * VGA ofrece un equilibrio entre:
   * - calidad;
   * - FPS;
   * - consumo de memoria;
   * - detalle para las enfermedades.
   */
  config.frame_size = FRAMESIZE_VGA;

  /*
   * Un valor menor significa mayor calidad.
   * 10: alta calidad, archivo pesado.
   * 12: equilibrio recomendado.
   * 15: más fluido, menor calidad.
   */
  config.jpeg_quality = 12;

  if (psramFound()) {
    Serial.println("PSRAM encontrada");

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    Serial.println("PSRAM no encontrada");

    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t error = esp_camera_init(&config);

  if (error != ESP_OK) {
    Serial.printf(
        "Error inicializando cámara: 0x%x\n",
        error
    );

    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();

  if (sensor == nullptr) {
    Serial.println("No se pudo obtener el sensor");
    return false;
  }

  // ===================================================
  // AJUSTES DE IMAGEN PARA HOJAS DE PALTA
  // ===================================================

  sensor->set_framesize(sensor, FRAMESIZE_VGA);

  // Ajustes básicos.
  sensor->set_brightness(sensor, 1);
  sensor->set_contrast(sensor, 0);
  sensor->set_saturation(sensor, 0);
  sensor->set_sharpness(sensor, 1);

  // Balance de blancos automático.
  sensor->set_whitebal(sensor, 1);
  sensor->set_awb_gain(sensor, 1);
  sensor->set_wb_mode(sensor, 0);

  // Exposición automática.
  sensor->set_exposure_ctrl(sensor, 1);
  sensor->set_aec2(sensor, 1);
  sensor->set_ae_level(sensor, 1);

  // Ganancia automática.
  sensor->set_gain_ctrl(sensor, 1);
  sensor->set_gainceiling(
      sensor,
      static_cast<gainceiling_t>(GAINCEILING_4X)
  );

  // Correcciones de imagen.
  sensor->set_bpc(sensor, 1);
  sensor->set_wpc(sensor, 1);
  sensor->set_raw_gma(sensor, 1);
  sensor->set_lenc(sensor, 1);
  sensor->set_dcw(sensor, 1);

  /*
   * Si la imagen aparece invertida:
   *
   * sensor->set_vflip(sensor, 1);
   *
   * Si aparece como espejo:
   *
   * sensor->set_hmirror(sensor, 1);
   */

  Serial.printf(
      "Sensor detectado, PID: 0x%02X\n",
      sensor->id.PID
  );

  return true;
}

// =====================================================
// CONEXIÓN WIFI
// =====================================================

bool conectarWiFi() {
  WiFi.mode(WIFI_STA);

  // Evita pausas ocasionadas por el ahorro de energía Wi-Fi.
  WiFi.setSleep(false);

  WiFi.begin(ssid, password);

  Serial.printf("Conectando a WiFi: %s", ssid);

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    // Tiempo máximo de conexión: 30 segundos.
    if (millis() - inicio > 30000) {
      Serial.println();
      Serial.println("No se pudo conectar al WiFi");

      return false;
    }
  }

  Serial.println();
  Serial.println("WiFi conectado");

  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Señal WiFi RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  return true;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  delay(1000);

  Serial.println();
  Serial.println("==================================");
  Serial.println("AgroVisión ESP32-CAM");
  Serial.println("==================================");

  configurarLed();

  if (!configurarCamara()) {
    Serial.println("La cámara no pudo inicializarse");
    return;
  }

  if (!conectarWiFi()) {
    Serial.println("No se pudo establecer conexión WiFi");
    return;
  }

  iniciarServidorCamara();

  String ip = WiFi.localIP().toString();

  Serial.println();
  Serial.println("==================================");
  Serial.println("ESP32-CAM lista");
  Serial.println("==================================");

  Serial.print("Panel web: http://");
  Serial.println(ip);

  Serial.print("Captura:   http://");
  Serial.print(ip);
  Serial.println("/capture");

  Serial.print("Estado:    http://");
  Serial.print(ip);
  Serial.println("/status");

  Serial.print("Stream:    http://");
  Serial.print(ip);
  Serial.println(":81/stream");

  Serial.println("==================================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  /*
   * El servidor web se ejecuta en otras tareas.
   * Aquí solamente comprobamos la conexión Wi-Fi.
   */

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Intentando reconectar...");

    WiFi.disconnect();
    WiFi.begin(ssid, password);

    unsigned long inicio = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - inicio < 15000
    ) {
      delay(500);
      Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi reconectado. IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("No se pudo recuperar la conexión");
    }
  }

  delay(10000);
}