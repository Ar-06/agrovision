import numpy as np
import tensorflow as tf
from keras.layers import Dense
from pathlib import Path


class DenseSinQuant(Dense):
    @classmethod
    def from_config(cls, config):

        config.pop("quantization_config", None)
        return super().from_config(config)
    
BACKEND_DIR = Path(__file__).resolve().parents[2]
MODELO_PATH = BACKEND_DIR / "models/modelo_palta_efficientnetb4_production.h5"

CLASS_NAMES = [
    "Bacterias",
    "Bichos",
    "Isariopsis",
    "Oidio",
    "Otras_Especies",
    "Sanas",
    "Septoria",
]

print("Cargando modelo EfficientNetB4, por favor espera...")

try:
    model = tf.keras.models.load_model(
        MODELO_PATH, custom_objects={"Dense": DenseSinQuant}
    )
    print("Modelo cargado exitosamente.")
except Exception as e:
    print(f"Error al cargar el modelo: {e}")
    model = None

def predecir_imagen(img_array: np.ndarray) -> tuple[str, float]:
    if model is None:
        raise RuntimeError("El modelo de IA no está disponible.")
    
    predicciones = model.predict(img_array, verbose=0)[0]
    indice_ganador = int(np.argmax(predicciones))

    clase_detectada = CLASS_NAMES[indice_ganador]
    confianza = float(predicciones[indice_ganador] * 100)

    return clase_detectada, confianza
