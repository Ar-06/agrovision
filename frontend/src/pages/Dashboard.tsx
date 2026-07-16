import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Progress } from "@/components/ui/progress";
import { useAgroVisionSocket } from "@/hooks/useAgrovisionSocket";
import { useDetectionHistory } from "@/hooks/useDetectionHistory";
import { useScanner } from "@/hooks/useScanner";
import {
  Activity,
  AlertTriangle,
  Camera,
  Clock3,
  History,
  Leaf,
  RefreshCw,
} from "lucide-react";
import { useEffect } from "react";

const API_BASE_URL = import.meta.env.VITE_API_URL;

const API_URL = `${API_BASE_URL}/predict`;

const WS_URL =
  import.meta.env.VITE_WS_URL;

const STREAM_URL =
  import.meta.env.VITE_ESP32_STREAM;

const CAPTURE_URL =
  import.meta.env.VITE_ESP32_CAPTURE;

export function DashBoard() {
  const {
  diagnostico,
  confianza,
  box,
  isScanning,
  capturedAndSend,
} = useScanner(
  CAPTURE_URL,
  API_URL,
);

  const {
    conectado,
    ultimaDeteccion,
  } = useAgroVisionSocket(WS_URL);

  const {
    history,
    isLoading: isHistoryLoading,
    error: historyError,
    reload: reloadHistory,
  } = useDetectionHistory(API_BASE_URL, ultimaDeteccion);

  const diagnosticoActual = ultimaDeteccion?.diagnostico ?? diagnostico;

  const confianzaActual = ultimaDeteccion?.confianza ?? confianza;

  const boxActual =
    ultimaDeteccion?.box ?? box;

  useEffect(() => {
  void capturedAndSend();

  const intervalId = window.setInterval(() => {
    void capturedAndSend();
  }, 5000);

  return () => {
    window.clearInterval(intervalId);
  };
}, [capturedAndSend]);

  const esperando =
    diagnosticoActual === "Esperando...";

  const hayError =
    diagnosticoActual === "Error de conexión" ||
    diagnosticoActual === "Tiempo agotado";

  const esSana =
    diagnosticoActual.toLowerCase() === "sanas";

  const hayEnfermedad =
    !esperando &&
    !hayError &&
    !esSana;

  const formatDate = (date: string | null) => {
    if (!date) {
      return "Sin fecha";
    }

    return new Intl.DateTimeFormat("es-CO", {
      day: "2-digit",
      month: "short",
      hour: "2-digit",
      minute: "2-digit",
    }).format(new Date(date));
  };

  const statusColor = esSana
  ? "bg-green-500"
  : hayEnfermedad
    ? "bg-red-500"
    : hayError
      ? "bg-amber-500"
      : "bg-slate-500";

  const boxColor = esSana
    ? "border-green-500"
    : hayEnfermedad
      ? "border-red-500"
      : hayError
        ? "border-amber-500"
        : "border-slate-500";

  return (
    <div className="min-h-screen bg-slate-950 p-6 text-slate-100 font-sans">
      <header className="mb-8 flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold tracking-tight text-white flex items-center gap-2">
            <Leaf className="text-emerald-500" /> Agrovisión Rover
          </h1>
          <p className="text-slate-400 mt-1">
            Panel de control y diagnóstico mediante IA
          </p>
        </div>
        <Badge
          variant="outline"
          className={
            conectado
              ? "px-4 py-1 text-emerald-400 border-emerald-400/30 bg-emerald-400/10"
              : "px-4 py-1 text-red-400 border-red-400/30 bg-red-400/10"
          }
        >
          <Activity
            className={`w-4 h-4 mr-2 ${conectado ? "animate-pulse" : ""}`}
          />

          {conectado ? "Sistema en línea" : "Sistema desconectado"}
        </Badge>
      </header>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        <Card className="lg:col-span-2 bg-slate-900 border-slate-800 shadow-xl overflow-hidden">
          <CardHeader className="border-b border-slate-800 bg-slate-900/50 pb-4">
            <CardTitle className="flex items-center gap-2 text-slate-200">
              <Camera className="w-5 h-5 text-blue-400" /> Cámara Principal
              {isScanning && (
                <span className="ml-auto text-xs text-blue-400 animate-pulse">
                  Escaneando frame...
                </span>
              )}
            </CardTitle>
          </CardHeader>
          <CardContent className="relative flex min-h-120 justify-center bg-black p-0">
            <div
              className="relative w-full max-w-160"
              style={{
                aspectRatio: "4 / 3",
              }}
            >
              <img
                src={STREAM_URL}
                alt="Stream ESP32-CAM"
                className="block h-full w-full object-contain"
              />

            {boxActual && (
              <div
                className={`absolute border-2 transition-all duration-300 ${boxColor}`}
                style={{
                  left: `${(boxActual.x / boxActual.image_width) * 100}%`,
                  top: `${(boxActual.y / boxActual.image_height) * 100}%`,
                  width: `${(boxActual.w / boxActual.image_width) * 100}%`,
                  height: `${(boxActual.h / boxActual.image_height) * 100}%`,
                }}
              >
                <span
                  className={`absolute -top-6 left-0 whitespace-nowrap px-2 py-0.5 text-xs font-bold text-white ${statusColor}`}
                >
                  {diagnosticoActual}
                </span>
              </div>
            )}

            {isScanning && (
              <div className="pointer-events-none absolute inset-0 overflow-hidden">
                <div className="absolute left-0 top-0 h-0.5 w-full animate-[scan_2s_linear_infinite] bg-blue-400 shadow-[0_0_12px_rgba(96,165,250,0.9)]" />
              </div>
            )}
            </div>
          </CardContent>
        </Card>

        <div className="space-y-6">
          <Card className="bg-slate-900 border-slate-800 shadow-xl">
            <CardHeader>
              <CardTitle className="text-slate-200">
                Análisis de Cultivo
              </CardTitle>
            </CardHeader>
            <CardContent className="space-y-6">
              <div className="flex flex-col items-center justify-center p-6 bg-slate-950 rounded-lg border border-slate-800">
                <p className="text-sm text-slate-400 mb-2">Estado Actual</p>
                <div
                  className={`text-3xl font-bold uppercase tracking-wider ${
                    esSana
                      ? "text-green-400"
                      : hayEnfermedad
                        ? "text-red-400"
                        : hayError
                          ? "text-amber-400"
                          : "text-slate-400"
                  }`}
                >
                  {diagnosticoActual}
                </div>
                {hayEnfermedad && (
                  <Badge
                    variant="destructive"
                    className="mt-3 flex items-center gap-1"
                  >
                    <AlertTriangle className="h-3 w-3" />
                    Patógeno detectado
                  </Badge>
                )}
              </div>

              <div className="space-y-2">
                <div className="flex justify-between text-sm">
                  <span className="text-slate-400">Nivel de Confianza</span>
                  <span className="font-mono text-slate-200">
                    {confianzaActual.toFixed(1)} %
                  </span>
                </div>
                <Progress
                  value={confianzaActual}
                  className={`h-2 ${esSana ? "[&>div]:bg-green-500" : "[&>div]:bg-red-500"}`}
                />
              </div>
            </CardContent>
          </Card>

          <Card className="bg-slate-900 border-slate-800 shadow-xl">
            <CardHeader className="flex flex-row items-center justify-between gap-3">
              <CardTitle className="flex items-center gap-2 text-slate-200">
                <History className="h-5 w-5 text-emerald-400" />
                Historial de Detecciones
              </CardTitle>
              <Button
                type="button"
                variant="outline"
                size="icon-sm"
                className="border-slate-700 bg-slate-950 text-slate-300 hover:bg-slate-800 hover:text-white"
                onClick={() => void reloadHistory()}
                disabled={isHistoryLoading}
                aria-label="Recargar historial"
              >
                <RefreshCw
                  className={isHistoryLoading ? "animate-spin" : ""}
                />
              </Button>
            </CardHeader>
            <CardContent>
              {historyError && (
                <div className="rounded-lg border border-amber-400/30 bg-amber-400/10 px-3 py-2 text-sm text-amber-300">
                  {historyError}
                </div>
              )}

              {!historyError && isHistoryLoading && (
                <div className="space-y-3">
                  {[1, 2, 3].map((item) => (
                    <div
                      key={item}
                      className="h-17 animate-pulse rounded-lg border border-slate-800 bg-slate-950"
                    />
                  ))}
                </div>
              )}

              {!historyError && !isHistoryLoading && history.length === 0 && (
                <div className="rounded-lg border border-slate-800 bg-slate-950 px-4 py-6 text-center text-sm text-slate-400">
                  Aún no hay detecciones guardadas.
                </div>
              )}

              {!historyError && !isHistoryLoading && history.length > 0 && (
                <div className="max-h-88 space-y-3 overflow-y-auto pr-1">
                  {history.map((detection) => {
                    const detectionIsHealthy =
                      detection.diagnostico.toLowerCase() === "sanas";

                    return (
                      <div
                        key={detection.id}
                        className="rounded-lg border border-slate-800 bg-slate-950 p-3"
                      >
                        <div className="flex items-start justify-between gap-3">
                          <div>
                            <p className="text-sm font-semibold uppercase tracking-wide text-slate-100">
                              {detection.diagnostico}
                            </p>
                            <p className="mt-1 flex items-center gap-1 text-xs text-slate-500">
                              <Clock3 className="h-3 w-3" />
                              {formatDate(detection.fecha_creacion)}
                            </p>
                          </div>

                          <Badge
                            variant="outline"
                            className={
                              detectionIsHealthy
                                ? "border-emerald-400/30 bg-emerald-400/10 text-emerald-300"
                                : "border-red-400/30 bg-red-400/10 text-red-300"
                            }
                          >
                            {detection.confianza.toFixed(1)}%
                          </Badge>
                        </div>
                      </div>
                    );
                  })}
                </div>
              )}
            </CardContent>
          </Card>

        </div>
      </div>
    </div>
  );
}
