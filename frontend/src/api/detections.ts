import type { PredictionResponse } from "@/hooks/useScanner";

export async function getDetectionHistory(
  apiBaseUrl: string,
  limit = 20,
) {
  const response = await fetch(
    `${apiBaseUrl}/detecciones?limite=${limit}`,
    {
      method: "GET",
      cache: "no-store",
    },
  );

  if (!response.ok) {
    const message = await response.text();

    throw new Error(
      `No se pudo cargar el historial: ${response.status} ${message}`,
    );
  }

  return (await response.json()) as PredictionResponse[];
}
