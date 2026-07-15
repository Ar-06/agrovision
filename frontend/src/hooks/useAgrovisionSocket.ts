import { useEffect, useState } from "react";
import type { PredictionResponse } from "./useScanner";

interface WebSocketMessage {
  tipo: "pong" | "nueva_deteccion";
  mensaje?: string;
  datos?: PredictionResponse;
}

export function useAgroVisionSocket(url: string) {
  const [conectado, setConectado] = useState(false);
  const [ultimaDeteccion, setUltimaDeteccion] =
    useState<PredictionResponse | null>(null);

  useEffect(() => {
    let socket: WebSocket;
    let reconnectTimeout: ReturnType<typeof setTimeout>;

    const conectar = () => {
      socket = new WebSocket(url);

      socket.onopen = () => {
        setConectado(true);
        socket.send("ping");
      };

      socket.onmessage = (event) => {
        const mensaje = JSON.parse(event.data) as WebSocketMessage;

        if (
          mensaje.tipo === "nueva_deteccion" &&
          mensaje.datos
        ) {
          setUltimaDeteccion(
            mensaje.datos as PredictionResponse,
          );
        }

      };

      socket.onerror = () => {
        socket.close();
      };

      socket.onclose = () => {
        setConectado(false);

        reconnectTimeout = setTimeout(() => {
          conectar();
        }, 3000);
      };
    };

    conectar();

    return () => {
      clearTimeout(reconnectTimeout);
      socket?.close();
    };
  }, [url]);

  return {
    conectado,
    ultimaDeteccion,
  };
}
