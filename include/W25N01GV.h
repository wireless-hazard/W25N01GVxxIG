#ifndef W25N_H
#define W25N_H

#ifdef __cplusplus
extern "C" {
#endif

#define W25_DEVICE_RESET 0xFF

typedef struct winbond winbond_t;

void vspi_start(void);
winbond_t *init_sensor(void);
int getSensorNota(winbond_t *sensor);


#ifdef __cplusplus
}
#endif

#endif