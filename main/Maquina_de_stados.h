// maquina_de_stados.h
#ifndef MAQUINA_DE_ESTADOS_H
#define MAQUINA_DE_ESTADOS_H
#include "stdbool.h"

typedef enum {
  RETRAIDO,  // en reposo, esperando a que se active el sistema
  EXTENDIDO, // los motores se accivan y posicionan los sensores en el lugar
  ERROR,     // error
  LEYENDO,
  LIBRE,
  OCUPADO

} estados_t;

typedef enum {
  STOP,
  START, // se activa el sistema
  MEASURE_TEMP,
  MEASURE_HUM,
  MEASURE_PH,
  MEASURE_CO2,
  MEASURE_DONE, // se han tomado las medidas
  SEND_DONE,    // se han enviado los datos a la nube
  DONE,
  ERROR_OCCURRED // ha ocurrido un error

} eventos_t;

#endif
