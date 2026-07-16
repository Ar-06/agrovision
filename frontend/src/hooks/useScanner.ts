import { useCallback, useRef, useState } from "react";

export interface BoxCoords {
  x: number;
  y: number;
  w: number;
  h: number;
  image_width: number;
  image_height: number;
}

export interface PredictionResponse {
  id: number;
  diagnostico: string;
  confianza: number;
  box: BoxCoords | null;
  fecha_creacion: string | null;
  error?: string;
}

export const useScanner = (
  captureUrl: string,
  apiUrl: string,
) => {
  const [diagnostico, setDiagnostico] =
    useState("Esperando...");

  const [confianza, setConfianza] = useState(0);

  const [box, setBox] =
    useState<BoxCoords | null>(null);

  const [isScanning, setIsScanning] =
    useState(false);

  const solicitudEnCurso = useRef(false);

  const capturedAndSend = useCallback(async () => {
    if (solicitudEnCurso.current) {
      return;
    }

    solicitudEnCurso.current = true;
    setIsScanning(true);

    const controller = new AbortController();

    const timeoutId = window.setTimeout(() => {
      controller.abort();
    }, 15000);

    try {
      /*
       * Solicitamos una fotografía nueva directamente
       * al endpoint /capture del ESP32-CAM.
       *
       * El timestamp evita que el navegador reutilice
       * una imagen guardada en caché.
       */
      const captureResponse = await fetch(
        `${captureUrl}?t=${Date.now()}`,
        {
          method: "GET",
          cache: "no-store",
          signal: controller.signal,
        },
      );

      if (!captureResponse.ok) {
        throw new Error(
          `El ESP32-CAM respondió con estado ${captureResponse.status}`,
        );
      }

      const contentType =
        captureResponse.headers.get("content-type");

      if (!contentType?.includes("image/jpeg")) {
        throw new Error(
          "El ESP32-CAM no devolvió una imagen JPEG",
        );
      }

      const imageBlob =
        await captureResponse.blob();

      if (imageBlob.size === 0) {
        throw new Error(
          "La captura recibida está vacía",
        );
      }

      const formData = new FormData();

      formData.append(
        "file",
        imageBlob,
        `captura-${Date.now()}.jpg`,
      );

      const predictionResponse = await fetch(
        apiUrl,
        {
          method: "POST",
          body: formData,
          signal: controller.signal,
        },
      );

      if (!predictionResponse.ok) {
        const mensaje =
          await predictionResponse.text();

        throw new Error(
          `FastAPI respondió ${predictionResponse.status}: ${mensaje}`,
        );
      }

      const data =
        (await predictionResponse.json()) as PredictionResponse;

      if (data.error) {
        throw new Error(data.error);
      }

      setDiagnostico(data.diagnostico);
      setConfianza(data.confianza);
      setBox(data.box);

    } catch (error) {
      if (
        error instanceof DOMException &&
        error.name === "AbortError"
      ) {
        console.error(
          "La captura o predicción excedió el tiempo límite",
        );

        setDiagnostico("Tiempo agotado");
      } else {
        console.error(
          "Error durante el escaneo:",
          error,
        );

        setDiagnostico("Error de conexión");
      }

      setConfianza(0);
      setBox(null);
    } finally {
      window.clearTimeout(timeoutId);

      solicitudEnCurso.current = false;
      setIsScanning(false);
    }
  }, [apiUrl, captureUrl]);

  return {
    diagnostico,
    confianza,
    box,
    isScanning,
    capturedAndSend,
  };
};
