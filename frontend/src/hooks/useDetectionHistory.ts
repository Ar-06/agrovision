import { getDetectionHistory } from "@/api/detections";
import { useCallback, useEffect, useState } from "react";
import type { PredictionResponse } from "./useScanner";

export function useDetectionHistory(
  apiBaseUrl: string,
  latestDetection: PredictionResponse | null,
) {
  const [history, setHistory] = useState<PredictionResponse[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadHistory = useCallback(async () => {
    try {
      const detections = await getDetectionHistory(apiBaseUrl);

      setHistory(detections);
      setError(null);
    } catch (requestError) {
      console.error("Error al cargar el historial:", requestError);
      setError("No se pudo cargar el historial");
    } finally {
      setIsLoading(false);
    }
  }, [apiBaseUrl]);

  useEffect(() => {
    const timeoutId = window.setTimeout(() => {
      void loadHistory();
    }, 0);

    return () => {
      window.clearTimeout(timeoutId);
    };
  }, [loadHistory]);

  const mergedHistory = latestDetection
    ? [
        latestDetection,
        ...history.filter(
          (detection) => detection.id !== latestDetection.id,
        ),
      ].slice(0, 20)
    : history;

  const reload = useCallback(async () => {
    setIsLoading(true);
    setError(null);

    await loadHistory();
  }, [loadHistory]);

  return {
    history: mergedHistory,
    isLoading,
    error,
    reload,
  };
}
