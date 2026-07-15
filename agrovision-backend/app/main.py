from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.api.routes import router
from app.database.connection import close_database, create_tables

from app.models.detection import Detection


@asynccontextmanager
async def lifespan(app: FastAPI):
    await create_tables()
    print("PostgreSQL conectado y tablas verificadas.")

    yield

    await close_database()

app = FastAPI(
    title="Agrovisión Rover API",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,  
    allow_methods=["*"], 
    allow_headers=["*"],
)


app.include_router(router)