
library(tseries)
library(astsa)
library(forecast)
library(foreign)
library(timsac)
library(vars)
library(mFilter)
library(dyn)
library(boot)
library(car)
library(QuantPsyc)
library(ggplot2)
library(scales)
library(dplyr) #Para imputar
library(tidyr)
library(naniar)
library(glue)
library(fs)
library(imputeTS) #Para imputar
library(xts)
library(lubridate) 
library(tidyverse)

################################################################################
# SCRIPT DE ANÁLISIS DE DATOS AMBIENTALES Y SEVERIDAD DE ROYA
# Instrucciones:
# 1. Abrir este script en R o RStudio
# 2. Asegurarse de que el archivo RDS esté en el mismo folder o modificar la ruta
# 3. Ejecutar el script por secciones para reproducir los análisis y gráficos

# # Establecer el directorio de trabajo donde están los datos
# setwd("C:/Users/NombreUsuario/MiProyecto")  # Cambiar esta ruta

# podés usar un file.choose() para que lo seleccione manualmente
# Datos_de_severidad <- readRDS(file.choose())

################################################################################


# Cargamos los datos de severidad 
Datos_de_severidad <- readRDS("Datos_de_severidad_completos.rds")
Datos_de_severidad
plot(Datos_de_severidad)

# library(dplyr)
# library(lubridate)
# 
# YearMonth_1 <- ymd(c("2024-04-02", "2024-05-22", "2024-06-15",
#                        "2024-07-02", "2024-08-29", "2024-09-28","2024-10-30",
#                        "2024-11-30", "2024-12-18"))
# 
# Datos_de_severidad <- Datos_de_severidad %>%
#   mutate(YearMonth = YearMonth_1)
# 
# Datos_de_severidad
#saveRDS(Datos_de_severidad, "Datos_de_severidad_completos.rds")



library(dplyr)
library(lubridate)



## _________ Mis datos parecen no tener una variación lineal - Spline cubico es el mejor método para esto ⤵_______________

# Creamos la secuencia de fechas semanales entre la fecha mínima y máxima
fechas_semanales <- seq(min(Datos_de_severidad$YearMonth), max(Datos_de_severidad$YearMonth), by = "7 days")

# Crear el spline cúbico con salida semanal
library(tidyverse) # Interpolación con spline cúbico
modelo_spline <- spline(x = as.numeric(Datos_de_severidad$YearMonth), # convertir fechas a números
                        y = Datos_de_severidad$promedio_porcentaje_mes,
                        xout = as.numeric(fechas_semanales))

Datos_de_severidad_2 <- tibble(
  fecha = as.Date(modelo_spline$x, origin = "1970-01-01"),
  severidad_interp = modelo_spline$y
)

plot(Datos_de_severidad_2)










## _________ 1. Cargo los datos que se imputaron  y guardamos en :"Data_para_correlacionar.rds" ⤵_________________________________________________________
Recopilacion_de_Roya_variables <- readRDS("Data_para_correlacionar.rds")
attach(Recopilacion_de_Roya_variables) 
names(Recopilacion_de_Roya_variables)
class(Hour)
class(Humidity)
class(Temperature)
class(Dewpoint)
summary(Recopilacion_de_Roya_variables)


## _________ 1.2. Verificamos que el set de datos sea el correcto y no tenga datos faltantes ⤵_______________
library(naniar) # para lidiar con missing values
miss_var_summary(Recopilacion_de_Roya_variables) # Tabla para ver el numero de missing por variable
head(Recopilacion_de_Roya_variables,5)
head(Datos_de_severidad,5)


# _________ 1.3 Visualización de las series de tiempo antes de tratar los outlayers ⤵____________________________________________________
library(ggplot2)
library(gridExtra)

# 1. Gráfico de Humedad
p1 <- ggplot(data = Recopilacion_de_Roya_variables, aes(x = Hour, y = Humidity)) +
  geom_line(color = 'black', size = 0.5, alpha = 1) +
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month",
                   limits = c(min(Recopilacion_de_Roya_variables$Hour), 
                              max(Recopilacion_de_Roya_variables$Hour)),
                   expand = c(0.01, 0)) +
  xlab('') + ylab('Humedad relativa (%)') +
  theme_minimal() +
  theme(axis.line = element_line(color = "black"), #ABCD1234
        axis.ticks = element_line(color = "black"),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank())

# 2. Gráfico de Punto de Rocío
p2 <- ggplot(data = Recopilacion_de_Roya_variables, aes(x = Hour, y = Dewpoint)) +
  geom_line(color = "black", size = 0.5, alpha = 1) +
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month",
                   limits = c(min(Recopilacion_de_Roya_variables$Hour), 
                              max(Recopilacion_de_Roya_variables$Hour)),
                   expand = c(0.01, 0)) +
  xlab('') + ylab('Punto de rocío (°C)') +
  theme_minimal() +
  theme(axis.line = element_line(color = "black"),
        axis.ticks = element_line(color = "black"),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank())

# 3. Gráfico de Temperatura
p3 <- ggplot(data = Recopilacion_de_Roya_variables, aes(x = Hour, y = Temperature)) +
  geom_line(color = "black", size = 0.5, alpha = 1) +
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month",
                   limits = c(min(Recopilacion_de_Roya_variables$Hour), 
                              max(Recopilacion_de_Roya_variables$Hour)),
                   expand = c(0.01, 0)) +
  xlab('') + ylab('Temperatura (°C)') +
  theme_minimal() +
  theme(axis.line = element_line(color = "black"),
        axis.ticks = element_line(color = "black"),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank())
grid.arrange(p1, p2, p3, ncol = 1) 





## _________ 2. Tratamiento de outlayers si los hay!: método del rango intercuartil (IQR) ⤵___________________________________________________________________________

# Función para detectar outliers con IQR
detect_outliers <- function(x) {
  Q1 <- quantile(x, 0.25, na.rm = TRUE)  # Primer cuartil
  Q3 <- quantile(x, 0.75, na.rm = TRUE)  # Tercer cuartil
  IQR <- Q3 - Q1  # Rango intercuartil
  lower_bound <- Q1 - 1.5 * IQR #Establece límites inferior y superior para detectar outliers
  upper_bound <- Q3 + 1.5 * IQR
  return(x < lower_bound | x > upper_bound)  # Devuelve TRUE si es outlier
}

# Aplicar la función a cada variable numérica
Recopilacion_de_Roya_variables$outlier_temp <- detect_outliers(Recopilacion_de_Roya_variables$Temperature)
Recopilacion_de_Roya_variables$outlier_hum <- detect_outliers(Recopilacion_de_Roya_variables$Humidity)
Recopilacion_de_Roya_variables$outlier_dew <- detect_outliers(Recopilacion_de_Roya_variables$Dewpoint)

# Ver filas con outliers
outliers_detected <- Recopilacion_de_Roya_variables[rowSums(Recopilacion_de_Roya_variables[, c("outlier_temp", "outlier_hum", "outlier_dew")]) > 0, ]

# Contar la cantidad de outliers en cada variable
outliers_temp <- sum(Recopilacion_de_Roya_variables$outlier_temp, na.rm = TRUE)
outliers_hum <- sum(Recopilacion_de_Roya_variables$outlier_hum, na.rm = TRUE)
outliers_dew <- sum(Recopilacion_de_Roya_variables$outlier_dew, na.rm = TRUE)
# Imprimir los resultados
cat("Outliers en Temperature:", outliers_temp, "\n")
cat("Outliers en Humidity:", outliers_hum, "\n")
cat("Outliers en Dewpoint:", outliers_dew, "\n")



## _________ 2.1. Graficamos todo para ver los puntos remarcados ⤵___________________________________________________________________________

library(ggplot2)
library(gridExtra)

# 1. Gráfico de Humedad con outliers resaltados
a1 <- ggplot(data = Recopilacion_de_Roya_variables, aes(x = Hour, y = Humidity)) +
  geom_line(color = 'black', size = 0.5, alpha = 1) +
  geom_point(data = Recopilacion_de_Roya_variables[Recopilacion_de_Roya_variables$outlier_hum, ],
             aes(x = Hour, y = Humidity), color = "black", shape = 1, size = 2) + 
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month",
                   limits = range(Recopilacion_de_Roya_variables$Hour),
                   expand = c(0.01, 0)) +
  xlab('') + ylab('Humedad relativa (%)') +
  theme_minimal()


# 2. Gráfico de Punto de Rocío con outliers resaltados
a2 <- ggplot(data = Recopilacion_de_Roya_variables, aes(x = Hour, y = Dewpoint)) +
  geom_line(color = 'black', size = 0.5, alpha = 1) +
  geom_point(data = Recopilacion_de_Roya_variables[Recopilacion_de_Roya_variables$outlier_dew, ],
             aes(x = Hour, y = Dewpoint), color = "black", shape = 1, size = 2) + 
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month",
                   limits = range(Recopilacion_de_Roya_variables$Hour),
                   expand = c(0.01, 0)) +
  xlab('') + ylab('Punto de rocío (°C)') +
  theme_minimal()


# 3. Gráfico de Temperatura con outliers resaltados
a3 <- ggplot(data = Recopilacion_de_Roya_variables, aes(x = Hour, y = Temperature)) +
  geom_line(color = 'black', size = 0.5, alpha = 1) +
  geom_point(data = Recopilacion_de_Roya_variables[Recopilacion_de_Roya_variables$outlier_temp, ],
             aes(x = Hour, y = Temperature), color = "black", shape = 1, size = 2) + 
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month",
                   limits = range(Recopilacion_de_Roya_variables$Hour),
                   expand = c(0.01, 0)) +
  xlab('Mes') + ylab('Temperatura (°C)') +
  theme_minimal()


# Combinar los gráficos       
grid.arrange(a1, a2, a3, ncol = 1)



## _________ 2.2 Visualización con boxplot ⤵____________________________________________________________

library(ggplot2)
library(gridExtra)  # Para organizar los gráficos en una figura

b1 <- ggplot(Recopilacion_de_Roya_variables, aes(y = Temperature)) +
  geom_boxplot(outlier.colour = "black", outlier.shape = 16, outlier.size = 3) +
  ylab("Temperatura (°C)") +
  theme_minimal()

b2 <- ggplot(Recopilacion_de_Roya_variables, aes(y = Dewpoint)) +
  geom_boxplot(outlier.colour = "black", outlier.shape = 16, outlier.size = 3) +
  ylab("Punto de Rocío (°C)") +
  theme_minimal()

b3 <- ggplot(Recopilacion_de_Roya_variables, aes(y = Humidity)) +
  geom_boxplot(outlier.colour = "black", outlier.shape = 16, outlier.size = 3) +
  ylab("Humedad (%)") +
  theme_minimal()
grid.arrange(b1, b2, b3, ncol = 2)












# _________ 3. CONVERTIR DATOS A SERIES .xts y ts para aplicar test ⤵__________________________________________________________
library(xts)
library(zoo)
library(lubridate)
library(forecast)

# --------------------  Datos horarios (xts) --------------------


# Convertir las fechas a POSIXct
serie_tiempo_T <- xts(Recopilacion_de_Roya_variables$Temperature, order.by = as.POSIXct(Recopilacion_de_Roya_variables$Hour))
serie_tiempo_H <- xts(Recopilacion_de_Roya_variables$Humidity, order.by = as.POSIXct(Recopilacion_de_Roya_variables$Hour))
serie_tiempo_D <- xts(Recopilacion_de_Roya_variables$Dewpoint, order.by = as.POSIXct(Recopilacion_de_Roya_variables$Hour))
serie_tiempo_R_2 <- xts(Datos_de_severidad_2$promedio_porcentaje_semana, order.by = as.POSIXct(Datos_de_severidad_2$YearMonth))


# Verificamos los formatos
class(serie_tiempo_T) # "xts" "zoo"
class(serie_tiempo_H) # "xts" "zoo"
class(serie_tiempo_D) # "xts" "zoo"
class(serie_tiempo_R) # "xts" "zoo"
class(serie_tiempo_R_2)












# _________ 4. DESCOMPOSICION STL ⤵__________________________________________________________
library(forecast)   # Para la función stl() y análisis de series de tiempo
library(ggplot2)    
library(tidyverse)

# Crear serie de tiempo con múltiples estacionalidades: diaria (24h), semanal (168h), mensual (~720h)
serie_msts_T <- msts(Recopilacion_de_Roya_variables$Temperature, start = c(2024, 1), seasonal.periods = c(24, 168)) 
serie_msts_H <- msts(Recopilacion_de_Roya_variables$Humidity, seasonal.periods = c(24, 168))
serie_msts_D <- msts(Recopilacion_de_Roya_variables$Dewpoint, seasonal.periods = c(24, 168))

class(serie_msts_T) # "msts" "ts"  

# 1. Tendencia (Trend): Representa la dirección general de la serie a largo plazo.
# 2. Estacionalidad (Seasonal): Captura los patrones cíclicos que se repiten en intervalos regulares (diarios, semanales, mensuales, etc.).
# 3. Residual (Ruido): Es lo que queda después de extraer la tendencia y la estacionalidad. Representa las fluctuaciones aleatorias o no sistemáticas en los datos.
# Descomposición usando mstl() en lugar de stl()
stl_T <- mstl(serie_msts_T, t.window = 720)# 720 es mensual osea 720 horas porque los datos son horarios
stl_H <- mstl(serie_msts_H, t.window = 720)
stl_D <- mstl(serie_msts_D, t.window = 720)
#stl_R <- mstl(serie_tiempo_R_2)



# Graficamos la descomposición
autoplot(stl_T) +
  scale_x_continuous(
    breaks = seq(2024, 2025, by = 1/12),  # cada mes
    labels = function(x) format(as.Date((x - 1970) * 365.25, origin = "1970-01-01"), "%b")
  ) +
  labs(x = "Mes")

autoplot(stl_T) + ggtitle("Descomposición de Temperatura")
autoplot(stl_H) + ggtitle("Descomposición de Humedad")
autoplot(stl_D) + ggtitle("Descomposición de Punto de Rocío")



# T_tendencia <- trendcycle(stl_T)
# H_tendencia <- trendcycle(stl_H)
# D_tendencia <- trendcycle(stl_D)
# plot(T_tendencia)
# plot(H_tendencia)
# plot(D_tendencia)
















# En este caso la serie de roya tenia mas semanas porque fue tomada desde antes, por lo que cuaqluier semana para atras que no coincida con el incio de la toma de datos de las variables ambientales es necesrio quitarla
Datos_de_severidad_2 <- Datos_de_severidad_2 %>%
  filter(fecha >= as.Date("2024-06-11")) # con filter podemos eliminar todos los datos anteriores a este fecha





# _________5. Ahora vamos a analizar correlacion cruzada semanal ⤵__________________________________________________________

library(dplyr)
library(lubridate)

# _________5.1 Para CCF ocupamos que tengan la misma frecuencia todas las series ⤵__________________________________________________________
library(dplyr)
library(lubridate)


# Fecha de corte es esta debido a que necesitamos que se agrupen los valores desde esta fecha e ir contando 7 dias para atras para obtner una varible que represente esa semana
fecha_final <- as.Date("2024-12-17")

# Calcular a qué semana pertenece cada fecha desde la fecha_final hacia atrás
Recopilacion_de_Roya_variables_semanal <- Recopilacion_de_Roya_variables %>%
  mutate(
    Fecha = as.Date(Hour),  # Convertir hora a fecha
    dias_diferencia = as.numeric(difftime(fecha_final, Fecha, units = "days")),
    semana_desde_fecha_final = dias_diferencia %/% 7,
    Semana = fecha_final - (semana_desde_fecha_final * 7),
    favorable_temp = ifelse(Temperature >= 20 & Temperature <= 24, 1, 0),
    alta_humedad = ifelse(Humidity > 90, 1, 0)
  ) %>%
  group_by(Semana) %>%
  summarise(
    Temperature_mean = mean(Temperature, na.rm = TRUE),
    Humidity_mean = mean(Humidity, na.rm = TRUE),
    Dewpoint_mean = mean(Dewpoint, na.rm = TRUE),
    Horas_favorables_temp = sum(favorable_temp, na.rm = TRUE),
    Horas_alta_humedad = sum(alta_humedad, na.rm = TRUE)
  ) %>%
  arrange(Semana)

count(Datos_de_severidad_2)




# ____ Del proceso anterior tenemos una semana extra, entonces la recortamos para ajustar la frecuencia con la otra series de roya
Recopilacion_de_Roya_variables_semanal = Recopilacion_de_Roya_variables_semanal %>%
  filter(Semana<= as.Date("2024-12-17"))



# Confirmamos que efectivamente estas posean la misma frecuencia y comprendan el mismo periodo
max(Recopilacion_de_Roya_variables_semanal$Semana)
min(Recopilacion_de_Roya_variables_semanal$Semana)
max(Datos_de_severidad_2$fecha)
min(Datos_de_severidad_2$fecha)

count(Recopilacion_de_Roya_variables_semanal)
count(Datos_de_severidad_2)







# _______________5.3 Ahora que tenemos los datos con la misma frecuencia semanal, tenemos que convertir estos datos a series de tiempo ⤵________________________

library(xts)
library(zoo)
library(lubridate)
library(forecast)

# --------------------  Datos horarios (xts) --------------------

# Convertir las fechas a POSIXct
serie_tiempo_T_PROM <- xts(Recopilacion_de_Roya_variables_semanal$Horas_favorables_temp, order.by = as.POSIXct(Recopilacion_de_Roya_variables_semanal$Semana))
serie_tiempo_H_PROM <- xts(Recopilacion_de_Roya_variables_semanal$Horas_alta_humedad, order.by = as.POSIXct(Recopilacion_de_Roya_variables_semanal$Semana))

serie_tiempo_T_COR <- xts(Recopilacion_de_Roya_variables_semanal$Temperature_mean, order.by = as.POSIXct(Recopilacion_de_Roya_variables_semanal$Semana))
serie_tiempo_H_COR <- xts(Recopilacion_de_Roya_variables_semanal$Humidity_mean, order.by = as.POSIXct(Recopilacion_de_Roya_variables_semanal$Semana))
serie_tiempo_D_COR <- xts(Recopilacion_de_Roya_variables_semanal$Dewpoint_mean, order.by = as.POSIXct(Recopilacion_de_Roya_variables_semanal$Semana))
serie_tiempo_R_2_COR <- xts(Datos_de_severidad_2$severidad_interp, order.by = as.POSIXct(Datos_de_severidad_2$fecha))


# Verificamos los formatos
class(serie_tiempo_T_COR) # "xts" "zoo"
class(serie_tiempo_H_COR) # "xts" "zoo"
class(serie_tiempo_D_COR) # "xts" "zoo"
class(serie_tiempo_R_2_COR) # "xts" "zoo"


# _________ 5.3.1 Visualización de las series de tiempo tipo xts ⤵____________________________________________________
library(ggplot2)
library(xts)
library(gridExtra)
library(ggfortify) # Para autoplot.xts

# 1. Gráfico de Humedad
g1 <- autoplot(serie_tiempo_H_COR) +
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month") +
  xlab('') + ylab('Humedad relativa (%)') +
  theme_minimal() +
  theme(axis.line = element_line(color = "black"),
        axis.ticks = element_line(color = "black"),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank())

# 2. Gráfico de Punto de Rocío
g2 <- autoplot(serie_tiempo_D_COR) +
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month") +
  xlab('') + ylab('Punto de rocío (°C)') +
  theme_minimal() +
  theme(axis.line = element_line(color = "black"),
        axis.ticks = element_line(color = "black"),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank())

# 3. Gráfico de Temperatura
g3 <- autoplot(serie_tiempo_T_COR) +
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month") +
  xlab('') + ylab('Temperatura (°C)') +
  theme_minimal() +
  theme(axis.line = element_line(color = "black"),
        axis.ticks = element_line(color = "black"),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank())

# 4. Gráfico de Severidad de Roya (Interpolada semanalmente)
g4 <- autoplot(serie_tiempo_R_2_COR) +
  scale_x_datetime(date_labels = "%b", date_breaks = "1 month") +
  xlab('') + ylab('Severidad Roya (%)') +
  theme_minimal() +
  theme(axis.line = element_line(color = "black"),
        axis.ticks = element_line(color = "black"),
        panel.grid.major = element_blank(),
        panel.grid.minor = element_blank())


grid.arrange(g1, g2, g3, g4, ncol = 1)







# _________ 2.1. Prueba de autocorrealcion para verficar estacionaridad de forma visual⤵___________________
library(tseries) # 1 semana de datos # LAG:  Si lo dejas por defecto, R usa lag.max = 10 * log10(n)

par(mfrow = c(2, 2)) # Establecer un diseño de 2 filas y 2 columnas para los gráficos
acf(coredata(serie_tiempo_R_2_COR), lag.max = 8, main = "ACF - Severidad de Roya") # 168 = 24 horas/día × 7 días/semana
acf(coredata(serie_tiempo_T_COR), lag.max = 8, main = "ACF - Punto de rocío")
acf(coredata(serie_tiempo_H_COR), lag.max = 8, main = "ACF - Temperatura")
acf(coredata(serie_tiempo_D_COR), lag.max = 8, main = "ACF - Humedad relativa ")
par(mfrow = c(1, 1)) # Volver a la configuración predeterminada de gráficoss
# _________ Nota el análisis mensual tendría sentido, pero con solo 7 meses, el patrón mensual podría no ser tan robusto.

par(mfrow = c(2, 2)) # Establecer un diseño de 2 filas y 2 columnas para los gráficos
acf(coredata(serie_tiempo_H_PROM), lag.max = 8, main = "ACF - Humedad alta") # 168 = 24 horas/día × 7 días/semana
acf(coredata(serie_tiempo_T_PROM), lag.max = 8, main = "ACF - Temperatura media")
par(mfrow = c(1, 1)) # Volver a la configuración predeterminada de gráficoss


# _________ Prueba Dick Fuller para las seires semanales
adf.test(serie_tiempo_R_2_COR) #p-value = 0.2393 ≥ 0.05, no se puede rechazar H₀ → la serie no es estacionaria.
adf.test(serie_tiempo_T_COR) # p-value = 0.1499
adf.test(serie_tiempo_H_COR) 
adf.test(serie_tiempo_D_COR)
adf.test(serie_tiempo_T_PROM)
adf.test(serie_tiempo_H_PROM)


kpss.test(serie_tiempo_R_2_COR)
kpss.test(serie_tiempo_T_COR)
kpss.test(serie_tiempo_H_COR)
kpss.test(serie_tiempo_D_COR)
kpss.test(serie_tiempo_H_PROM)
kpss.test(serie_tiempo_T_PROM)



library(forecast) # Usa Kpss por lo que pueden haber diferencias entre esta y ADF 

# Calcular número de diferenciaciones necesarias para cada serie
ndiffs(serie_tiempo_R_2_COR)  # Severidad
ndiffs(serie_tiempo_T_COR)    # Temperatura
ndiffs(serie_tiempo_H_COR)    # Humedad relativa
ndiffs(serie_tiempo_D_COR)    # Punto de rocío


# _________ NOTA antes 

# _________ Conocemos que son series no estacionarias, pero estas pueden COINTEGRAS
# _________ Es decir entre ellas existe una combinación  lineal que si es estacionaria
# _________ la cual es la serie pero de los residuales de estas


# Estimamos la regresion lineal entre estas
modelo_T_coin <- lm(serie_tiempo_R_2_COR ~ serie_tiempo_T_COR)
modelo_H_coin <- lm(serie_tiempo_R_2_COR ~ serie_tiempo_H_COR)
modelo_D_coin <- lm(serie_tiempo_R_2_COR ~ serie_tiempo_D_COR)

# Obtenemos los residuales del modelo 
residuales_T <- resid(modelo_T_coin)
residuales_H <- resid(modelo_H_coin)
residuales_D <- resid(modelo_D_coin)
# Veriricamos estacionariedad de los residuales
adf.test(residuales_T)
adf.test(residuales_H)
adf.test(residuales_D)


# _________ Veamos si la serie posee una tendencia deterministica osea una linea recto o alguna curva suave predecible

# Crear variable de tiempo (índice numérico)
tiempo <- 1:length(serie_tiempo_R_2_COR)

# Ajustar modelo de tendencia lineal
modelo_trend_Prueba_deter <- lm(serie_tiempo_R_2_COR ~ tiempo)
residuales_R_T <- residuals(modelo_trend_Prueba_deter)
adf.test(residuales_R_T)






# _________ Ahora que sabemos que todos son procesos estocasticos Aplicamos la diferenciación

library(tseries)

# Aplicar diferenciación de orden 1 a las series
serie_tiempo_R_2_COR_diff <- diff(serie_tiempo_R_2_COR, differences = 1)
serie_tiempo_T_COR_diff <- diff(serie_tiempo_T_COR, differences = 1)
serie_tiempo_H_COR_diff <- diff(serie_tiempo_H_COR, differences = 1)
serie_tiempo_D_COR_diff <- diff(serie_tiempo_D_COR, differences = 1)

serie_tiempo_H_PROM_diff <- diff(serie_tiempo_H_PROM, differences = 1)
serie_tiempo_T_PROM_diff <- diff(serie_tiempo_T_PROM, differences = 1)


# Eliminar NA del inicio
serie_tiempo_R_2_COR_diff <- na.omit(serie_tiempo_R_2_COR_diff)
serie_tiempo_T_COR_diff <- na.omit(serie_tiempo_T_COR_diff)
serie_tiempo_H_COR_diff <- na.omit(serie_tiempo_H_COR_diff)
serie_tiempo_D_COR_diff <- na.omit(serie_tiempo_D_COR_diff)


serie_tiempo_H_PROM_diff <- na.omit(serie_tiempo_H_PROM_diff)
serie_tiempo_T_PROM_diff <- na.omit(serie_tiempo_T_PROM_diff)

# Verificar estacionaridad después de la diferenciación
adf.test(serie_tiempo_R_2_COR_diff)
adf.test(serie_tiempo_T_COR_diff)
adf.test(serie_tiempo_H_COR_diff)
adf.test(serie_tiempo_D_COR_diff)

adf.test(serie_tiempo_T_PROM_diff)
adf.test(serie_tiempo_H_PROM_diff)


kpss.test(serie_tiempo_R_2_COR_diff)
kpss.test(serie_tiempo_T_COR_diff)
kpss.test(serie_tiempo_H_COR_diff)
kpss.test(serie_tiempo_D_COR_diff)

# head(serie_tiempo_R_2_COR_diff)
# head(serie_tiempo_T_COR_diff)
# head(serie_tiempo_H_COR_diff)
# head(serie_tiempo_D_COR_diff)




# Segunda diferenciación
# Es más limpio aplicar la segunda diferenciación debido 
serie_tiempo_R_2_COR_diff2 <- diff(serie_tiempo_R_2_COR, differences = 2)
serie_tiempo_T_COR_diff2 <-diff(serie_tiempo_T_COR, differences = 2)
serie_tiempo_H_COR_diff2 <- diff(serie_tiempo_H_COR, differences = 2)
serie_tiempo_D_COR_diff2 <- diff(serie_tiempo_D_COR, differences = 2)

serie_tiempo_H_PROM_diff2 <- diff(serie_tiempo_H_PROM, differences = 2)
serie_tiempo_T_PROM_diff2 <- diff(serie_tiempo_T_PROM, differences = 2)


serie_tiempo_R_2_COR_diff2 <- na.omit(serie_tiempo_R_2_COR_diff2)
serie_tiempo_T_COR_diff2 <- na.omit(serie_tiempo_T_COR_diff2)
serie_tiempo_H_COR_diff2 <- na.omit(serie_tiempo_H_COR_diff2)
serie_tiempo_D_COR_diff2 <- na.omit(serie_tiempo_D_COR_diff2)

serie_tiempo_H_PROM_diff2 <- na.omit(serie_tiempo_H_PROM_diff2)
serie_tiempo_T_PROM_diff2 <- na.omit(serie_tiempo_T_PROM_diff2)


# Verificar estacionaridad después de la diferenciación
adf.test(serie_tiempo_R_2_COR_diff2)
adf.test(serie_tiempo_T_COR_diff2)
adf.test(serie_tiempo_H_COR_diff2)
adf.test(serie_tiempo_D_COR_diff2)

adf.test(serie_tiempo_T_PROM_diff2)
adf.test(serie_tiempo_H_PROM_diff2)

kpss.test(serie_tiempo_R_2_COR_diff)
kpss.test(serie_tiempo_T_COR_diff2)
kpss.test(serie_tiempo_H_COR_diff2)
kpss.test(serie_tiempo_D_COR_diff2)

head(serie_tiempo_R_2_COR_diff2)
head(serie_tiempo_T_COR_diff2)
head(serie_tiempo_H_COR_diff2)
head(serie_tiempo_D_COR_diff2)




# Para aplicar una correlación cruzada (CCF) entre dos series, ambas deben tener la misma longitud y estar alineadas temporalmente.


#________Para el presente análisis en el cual buscamos si las variables ambientales explican a la roya, 
#________lo correcto es poner la roya como x (primera) y la variable ambiental como y en la prueba cccf.


# lag.max = 20: establece el máximo rezago a explorar



ccf(
  as.numeric(serie_tiempo_T_COR_diff2),
  as.numeric(serie_tiempo_R_2_COR_diff2),
  lag.max = 10,
  main = "Correlación cruzada: Severidad vs Temperatura"
)



# Correlación cruzada: Severidad vs Humedad
ccf(
  as.numeric(serie_tiempo_H_COR_diff2),
  as.numeric(serie_tiempo_R_2_COR_diff2),
  lag.max = 10,
  main = "Correlación cruzada: Severidad vs Humedad"
)

# Correlación cruzada: Severidad vs Punto de rocío
ccf(
  as.numeric(serie_tiempo_D_COR_diff2),
  as.numeric(serie_tiempo_R_2_COR_diff2),
  lag.max = 10,
  main = "Correlación cruzada: Severidad vs Punto de rocío"
)


length(serie_tiempo_R_2_COR_diff2)  # Número de observaciones
length(serie_tiempo_D_COR_diff2)




# Correlación con los valores promedios, pruebas
ccf(
  as.numeric(serie_tiempo_T_PROM_diff2),
  as.numeric(serie_tiempo_R_2_COR_diff2),
  lag.max = 10,
  main = "Correlación cruzada: Severidad vs Temperatura"
)


ccf(
  as.numeric(serie_tiempo_H_PROM_diff2),
  as.numeric(serie_tiempo_R_2_COR_diff2),
  lag.max = 10,
  main = "Correlación cruzada: Severidad vs Temperatura"
)



ccf(
  as.numeric(serie_tiempo_T_COR),
  as.numeric(serie_tiempo_R_2_COR),
  lag.max = 15,
  main = "Correlación cruzada: Severidad vs Temperatura"
)




# _________ 2. EVALUACIÓN DE SUPUESTOS PARA CONFIRMAR⤵___________________________________________________________________

# El test está comparando los valores de la serie en función del ciclo detectado automáticamente.
# REFERENCIA
# https://cdn.www.gob.pe/uploads/document/file/1200275/Combinaci%C3%B3n_de_metodolog%C3%ADas_de_predicci%C3%B3n_para_el_sector_de_las_telecomunicaciones.pdf?utm_source=chatgpt.com

# Semanal
kruskal.test(as.numeric(serie_tiempo_T) ~ format(index(serie_tiempo_T), "%W")) # p-value < 2.2e-16 (< 0.05), indica que la serie es estacional
kruskal.test(as.numeric(serie_tiempo_H) ~ format(index(serie_tiempo_H), "%W")) # p-value < 2.2e-16
kruskal.test(as.numeric(serie_tiempo_D) ~ format(index(serie_tiempo_D), "%W")) # p-value < 2.2e-16

# diaria 
kruskal.test(as.numeric(serie_tiempo_T) ~ format(index(serie_tiempo_T), "%H")) # p-value < 2.2e-16
kruskal.test(as.numeric(serie_tiempo_H) ~ format(index(serie_tiempo_H), "%H")) # p-value < 2.2e-16
kruskal.test(as.numeric(serie_tiempo_D) ~ format(index(serie_tiempo_D), "%H")) # p-value < 2.2e-16

# _________ Nota  Kruskall se basa en comparar distribuciones entre grupos, y con solo 7 grupos (meses), la variabilidad podría no ser suficiente para obtener conclusiones sólidas
kruskal.test(as.numeric(serie_tiempo_R) ~ format(index(serie_tiempo_R), "%m")) #  p-value = 0.4335 (> 0.05), no hay evidencia de estacionalidad significativa.
# Esos resultados deben tomarse con cautela debido a la poca cantidad de meses.


# _________ 2.2 Aplicar la prueba de Mann-Kendall para ver la tendendencia ⤵_______________________________________

# 1. Tendencia Monótona: El método Mann-Kendall asume que la tendencia en la serie temporal es monótona
# 2. método se puede aplicar a datos de mayor frecuencia (mensual, diaria, horaria) siempre que se manejen adecuadamente los patrones estacionales y otros ciclos.
# 3 Tendencias No Lineales: Si la tendencia no es estrictamente monótona, los resultados pueden ser menos confiables
library(trend)
library(Kendall)
library(zyp)
library(xts)
# REFERENCIA
# https://upcommons.upc.edu/bitstream/handle/2099.1/25959/Evoluci%c3%b3n%20temporal%20de%20rendimiento%20de%20cultivos%20y%20su%20relaci%c3%b3n%20con%20la%20se%c3%b1al%20clim%c3%a1tica%20en%20Tarragona_Aida%20Garcia%20Boria.pdf?sequence=1&isAllowed=y
mk.test(coredata(serie_tiempo_R)) # p-value = 0.07633 > 0.05 Posible tendencia creciente, pero no significativa (cerca del 0.05).
mk.test(coredata(serie_tiempo_H)) # p-value < 2.2e-16 (< 0.05) Tendencia creciente y significativa.
mk.test(coredata(serie_tiempo_D)) # p-value < 2.2e-16 (< 0.05) Tendencia creciente y significativa.
mk.test(coredata(serie_tiempo_T)) # p-value = 3.557e-10 (< 0.05) Tendencia decreciente y significativa.





# ___ Ahora bien podemos estudair las Variables acumuladas para estudiar la severidad de roya (con rezagos) ⤵_______________________________________

# Vamos a tener variables discretas osea el conteo de eventos como un valor numerico
head(Recopilacion_de_Roya_variables)
head(Datos_de_severidad_2)

class(Datos_de_severidad_2$fecha)
class(Recopilacion_de_Roya_variables$Hour)



library(dplyr)
library(lubridate)
library(tidyr)
library(purrr)



# Creamos función para calcular horas con condiciones específicas en un rango
contar_horas_condicion <- function(fecha_severidad, dias_rezago, clima_df) {
  fecha_inicio <- fecha_severidad - days(dias_rezago)
  clima_df %>%
    filter(Hour >= fecha_inicio & Hour < fecha_severidad) %>%
    filter(Temperature >= 21, Temperature <= 25, Humidity > 90) %>%
    nrow() # número de horas que cumplen la condición
}

# Aplicamos la función para cada fila de Datos_de_severidad_2 y múltiples rezagos
ventanas_1 <- c(7, 15, 22, 30)

# Usamos purrr para hacer el cálculo de manera funcional
Resultados <- Datos_de_severidad_2 %>%
  mutate(across(.cols = everything(), .fns = ~ ., .names = "{.col}")) %>%
  rowwise() %>%
  mutate(acumulados = list(
    map_dbl(ventanas_1, ~ contar_horas_condicion(fecha, .x, Recopilacion_de_Roya_variables))
  )) %>%
  unnest_wider(acumulados, names_sep = "_") %>%
  rename_with(~ paste0("Horas_T21_25_HR90_", ventanas_1, "d"), starts_with("acumulados_"))

# Resultado
print(Resultados)






library(dplyr)
library(lubridate)
library(tidyr)
library(purrr)
#___________La Condensación visible se da cuando Td ≈ T, típicamente cuando la humedad relativa se acerca al 100%


# Estrictamente T rocio nunca sera mayor a T, pero podemos considera en la practica que 
# con un punto de rocío cercano a la temperatura noraml y humedad relativa muy 
# alta— obtendríamos una estimación más realista del riesgo de condensación en la hoja.


# Función para contar horas donde hay posible condensación (Td ≥ T)
contar_horas_condensacion <- function(fecha_severidad, dias_rezago, clima_df) {
  fecha_inicio <- fecha_severidad - days(dias_rezago)
  
  clima_df %>%
    filter(Hour >= fecha_inicio & Hour < fecha_severidad) %>%
    filter(Dewpoint >= Temperature - 0.5, Humidity >= 98) %>%
    nrow()  # número de horas que cumplen ambas condiciones
}

# Aplicamos para cada fecha de severidad y cada ventana
Resultados_condensacion <- Datos_de_severidad_2 %>%
  rowwise() %>%
  mutate(condensacion = list(
    map_dbl(ventanas_1, ~ contar_horas_condensacion(fecha, .x, Recopilacion_de_Roya_variables))
  )) %>%
  unnest_wider(condensacion, names_sep = "_") %>%
  rename_with(~ paste0("Horas_Td_gte_T_", ventanas_1, "d"), starts_with("condensacion_"))

# Vista del resultado
print(Resultados_condensacion)



library(dplyr)

tabla_completa <- Resultados %>%
  inner_join(Resultados_condensacion, by = c("fecha", "severidad_interp"))
head(tabla_completa)

library(dplyr)

tabla_completa <- tabla_completa %>%
  rename(
    severidad = severidad_interp,
    T21_25_HR90_7d = Horas_T21_25_HR90_7d,
    T21_25_HR90_15d = Horas_T21_25_HR90_15d,
    T21_25_HR90_22d = Horas_T21_25_HR90_22d,
    T21_25_HR90_30d = Horas_T21_25_HR90_30d,
    Td_gte_T_7d = Horas_Td_gte_T_7d,
    Td_gte_T_15d = Horas_Td_gte_T_15d,
    Td_gte_T_22d = Horas_Td_gte_T_22d,
    Td_gte_T_30d = Horas_Td_gte_T_30d
  )

# Seleccionamos las variables climáticas y la severidad
variables <- tabla_completa %>%
  select(-fecha)  # quitamos la fecha para correlación

head(variables)

# Correlación de Pearson
cor_pearson <- cor(variables, method = "pearson")

# Correlación de Spearman (rango)
cor_spearman <- cor(variables, method = "spearman")

print(cor_pearson)
print(cor_spearman)


library(corrplot)

# Correlación de Pearson
corrplot(cor_pearson,
         method = "color",
         title = "Correlación de Pearson",
         mar = c(0, 0, 2, 0),
         tl.pos = "lt",          # Títulos a la izquierda y abajo
         tl.col = "black",       # Color de los textos
         tl.cex = 0.8,           # Tamaño del texto
         addCoef.col = "black",  # Agrega los valores numéricos en negro
         number.digits = 2,
         number.cex = 0.8)       # Tamaño de los números

# Correlación de Spearman
corrplot(cor_spearman,
         method = "color",
         title = "Correlación de Spearman",
         mar = c(0, 0, 2, 0),
         tl.pos = "lt",
         tl.col = "black",
         tl.cex = 0.8,
         addCoef.col = "black",
         number.digits = 2,
         number.cex = 0.8)





















# ____________________21-25___________________________________________________________________________-

Datos_de_severidad_3 <- Datos_de_severidad[-c(1, 2), ] # sacamos este par de datos, porque empezamos a tomar datos desde el mes 6
names(Datos_de_severidad_3)[1] <- "fecha" # tenia el nombre yearmonth y se lo cambiamos

library(dplyr)
library(lubridate)
library(tidyr)
library(purrr)



# Creamos función para calcular horas con condiciones específicas en un rango
contar_horas_condicion_3 <- function(fecha_severidad, dias_rezago, clima_df) { # parametro
  fecha_inicio <- fecha_severidad - days(dias_rezago)
  clima_df %>%
    filter(Hour >= fecha_inicio & Hour < fecha_severidad) %>%
    filter(Temperature >= 21, Temperature <= 25, Humidity > 87) %>%
    nrow() # número de horas que cumplen la condición
}

# Aplicamos la función para cada fila de Datos_de_severidad_2 y múltiples rezagos
ventanas <- c(11, 13, 15 , 18 ,20, 22,25, 27,30,32)

# Usamos purrr para hacer el cálculo de manera funcional
Resultados_3 <- Datos_de_severidad_3 %>%
  mutate(across(.cols = everything(), .fns = ~ ., .names = "{.col}")) %>%
  rowwise() %>%
  mutate(acumulados = list(
    map_dbl(ventanas, ~ contar_horas_condicion_3(fecha, .x, Recopilacion_de_Roya_variables))
  )) %>%
  unnest_wider(acumulados, names_sep = "_") %>%
  rename_with(~ paste0("T_21-25_HR-90_", ventanas, "d"), starts_with("acumulados_"))

# Resultado
print(Resultados_3)
Resultados_3 <- Resultados_3 %>%
  rename(
    Severidad = promedio_porcentaje_mes
  )



library(dplyr)
variables_3 <- Resultados_3[, -which(names(Resultados_3) == "fecha")]


head(Resultados_3)

# Correlación de Pearson
cor_pearson_3 <- cor(variables_3, method = "pearson")

# Correlación de Spearman (rango)
cor_spearman_3 <- cor(variables_3, method = "spearman")


library(corrplot)
# Correlación de Spearman
corrplot(cor_spearman_3,
         method = "color",
         title = "Correlación de Spearman 21-25",
         mar = c(0, 0, 2, 0),
         tl.pos = "lt",
         tl.col = "black",
         tl.cex = 0.8,
         addCoef.col = "black",
         number.digits = 2,
         number.cex = 0.8)











# _____________18-21___________________________________________________________
library(dplyr)
library(lubridate)
library(tidyr)
library(purrr)


# Creamos función para calcular horas con condiciones específicas en un rango
contar_horas_condicion_4 <- function(fecha_severidad, dias_rezago, clima_df) {
  fecha_inicio <- fecha_severidad - days(dias_rezago)
  clima_df %>%
    filter(Hour >= fecha_inicio & Hour < fecha_severidad) %>%
    filter(Temperature >= 18, Temperature <= 21, Humidity >=90) %>%
    nrow() # número de horas que cumplen la condición
}

# Aplicamos la función para cada fila de Datos_de_severidad_2 y múltiples rezagos
ventanas_4 <- c(11, 13, 15, 18, 20, 22, 25, 27, 30, 32)

# Usamos purrr para hacer el cálculo de manera funcional
Resultados_4 <- Datos_de_severidad_3 %>%
  mutate(across(.cols = everything(), .fns = ~ ., .names = "{.col}")) %>%
  rowwise() %>%
  mutate(acumulados = list(
    map_dbl(ventanas_4, ~ contar_horas_condicion_4(fecha, .x, Recopilacion_de_Roya_variables))
  )) %>%
  unnest_wider(acumulados, names_sep = "_") %>%
  rename_with(~ paste0("T_18-21_HR-90_", ventanas_4, "d"), starts_with("acumulados_"))


# Resultado
print(Resultados_4)
Resultados_4 <- Resultados_4 %>%
  rename(
    Severidad = promedio_porcentaje_mes
  )


library(dplyr) 
variables_4 <- Resultados_4[, -which(names(Resultados_3) == "fecha")]
cor_pearson_4 <- cor(variables_4, method = "pearson")
cor_spearman_4 <- cor(variables_4, method = "spearman")


library(corrplot) # Correlación de Spearman
corrplot(cor_spearman_4,
         method = "color",
         title = "Correlación de Spearman 18-21",
         # title = "Correlación de Spearman 21-23",
         # # title = "Correlación de Spearman 23-25",
         mar = c(0, 0, 2, 0),
         tl.pos = "lt",
         tl.col = "black",
         tl.cex = 0.8,
         addCoef.col = "black",
         number.digits = 2,
         number.cex = 0.8)






# _____________21-23__________________________________________________________________________________-

library(dplyr)
library(lubridate)
library(tidyr)
library(purrr)

# Creamos función para calcular horas con condiciones específicas en un rango
contar_horas_condicion_4 <- function(fecha_severidad, dias_rezago, clima_df) {
  fecha_inicio <- fecha_severidad - days(dias_rezago)
  clima_df %>%
    filter(Hour >= fecha_inicio & Hour < fecha_severidad) %>%
    filter(Temperature >= 21, Temperature <= 23, Humidity >=90) %>%
    nrow() # número de horas que cumplen la condición
}

# Aplicamos la función para cada fila de Datos_de_severidad_2 y múltiples rezagos
ventanas_4 <- c(11, 13, 15 , 18 ,20, 22,25, 27,30,32)

# Usamos purrr para hacer el cálculo de manera funcional
Resultados_4 <- Datos_de_severidad_3 %>%
  mutate(across(.cols = everything(), .fns = ~ ., .names = "{.col}")) %>%
  rowwise() %>%
  mutate(acumulados = list(
    map_dbl(ventanas_4, ~ contar_horas_condicion_4(fecha, .x, Recopilacion_de_Roya_variables))
  )) %>%
  unnest_wider(acumulados, names_sep = "_") %>%
  rename_with(~ paste0("T_21-23_HR-90_", ventanas_4, "d"), starts_with("acumulados_"))

              
# Resultado
print(Resultados_4)
Resultados_4 <- Resultados_4 %>%
  rename(
    Severidad = promedio_porcentaje_mes
  )

library(dplyr)
variables_4 <- Resultados_4[, -which(names(Resultados_3) == "fecha")]
cor_spearman_4 <- cor(variables_4, method = "spearman")

library(corrplot)
# Correlación de Spearman
corrplot(cor_spearman_4,
         method = "color",
         # title = "Correlación de Spearman 18-21",
         title = "Correlación de Spearman 21-23",
         # title = "Correlación de Spearman 23-25",
         mar = c(0, 0, 2, 0),
         tl.pos = "lt",
         tl.col = "black",
         tl.cex = 0.8,
         addCoef.col = "black",
         number.digits = 2,
         number.cex = 0.8)


# _____________23-25____________________________________________________________
library(dplyr)
library(lubridate)
library(tidyr)
library(purrr)

# Creamos función para calcular horas con condiciones específicas en un rango
contar_horas_condicion_4 <- function(fecha_severidad, dias_rezago, clima_df) {
  fecha_inicio <- fecha_severidad - days(dias_rezago)
  clima_df %>%
    filter(Hour >= fecha_inicio & Hour < fecha_severidad) %>%
    filter(Temperature >=23, Temperature <=25, Humidity >=90) %>%
    nrow() # número de horas que cumplen la condición
}

# Aplicamos la función para cada fila de Datos_de_severidad_2 y múltiples rezagos
ventanas_4 <- c(11, 13, 15 , 18 ,20, 22,25, 27,30,32)

# Usamos purrr para hacer el cálculo de manera funcional
Resultados_4 <- Datos_de_severidad_3 %>%
  mutate(across(.cols = everything(), .fns = ~ ., .names = "{.col}")) %>%
  rowwise() %>%
  mutate(acumulados = list(
    map_dbl(ventanas_4, ~ contar_horas_condicion_4(fecha, .x, Recopilacion_de_Roya_variables))
  )) %>%
  unnest_wider(acumulados, names_sep = "_") %>%
  rename_with(~ paste0("T_23-25_HR-90_", ventanas_4, "d"), starts_with("acumulados_"))

# Resultado
print(Resultados_4)
Resultados_4 <- Resultados_4 %>%
  rename(
    Severidad = promedio_porcentaje_mes
  )

library(dplyr)
variables_4 <- Resultados_4[, -which(names(Resultados_3) == "fecha")]
cor_spearman_4 <- cor(variables_4, method = "spearman")


library(corrplot)
corrplot(cor_spearman_4,   # Correlación de Spearman
         method = "color",
         title = "Correlación de Spearman 23-25",
         mar = c(0, 0, 2, 0),
         tl.pos = "lt",
         tl.col = "black",
         tl.cex = 0.8,
         addCoef.col = "black",
         number.digits = 2,
         number.cex = 0.8)


















# _______________________________________________________________________________________________-

# 1) Carga de paquetes y definición de ventanas
library(dplyr)
library(lubridate)
library(tidyr)
library(purrr)
#___________La Condensación visible se da cuando Td ≈ T, típicamente cuando la humedad relativa se acerca al 100%


# Estrictamente T rocio nunca sera mayor a T, pero podemos considera en la practica que 
# con un punto de rocío cercano a la temperatura noraml y humedad relativa muy 
# alta— obtendríamos una estimación más realista del riesgo de condensación en la hoja.
ventanas_2 <- c(7,10, 13, 15 , 18 ,20, 22,25, 27,30)

# 2) Función para contar horas donde hay posible condensación (Td ≥ T)
#fecha_severidad: la fecha en que mediste severidad de roya.
#dias_rezago: cuántos días hacia atrás mirás.
#clima_df: base de datos horaria 

contar_horas_condensacion_3<- function(fecha_severidad, dias_rezago, clima_df) {
  fecha_inicio <- fecha_severidad - days(dias_rezago)
  
  clima_df %>%
    filter(Hour >= fecha_inicio & Hour < fecha_severidad) %>%
    filter(Temperature >= 21,Temperature <= 25, Dewpoint >= Temperature - 0.5, Humidity >= 98) %>%
    nrow()  # número de horas que cumplen ambas condiciones
}

# Aplicamos para cada fecha de severidad y cada ventana
Resultados_condensacion_3<- Datos_de_severidad_3 %>%
  rowwise() %>%
  mutate(condensacion = list(
    map_dbl(ventanas_2, ~ contar_horas_condensacion_3(fecha, .x, Recopilacion_de_Roya_variables))
  )) %>%
  unnest_wider(condensacion, names_sep = "_") %>%
  rename_with(~ paste0("T_21-25_HR-98_Dew_", ventanas_2, "d"), starts_with("condensacion_"))



# Vista del resultado
print(Resultados_condensacion_3)
colnames(Resultados_condensacion_3)





# 4) Preparar tabla para correlación
library(dplyr)

tabla_codensacion_3 <- Resultados_condensacion_3 %>%
  rename(
    severidad = promedio_porcentaje_mes,
  )

# Seleccionamos las variables climáticas y la severidad
variable_condensa <- tabla_codensacion_3 %>%
  select(-fecha)  # quitamos la fecha para correlación

head(variable_condensa)

# Correlación de Pearson
cor_pearson_cond <- cor(variable_condensa, method = "pearson")

# Correlación de Spearman (rango)
cor_spearman_cond <- cor(variable_condensa, method = "spearman")

print(cor_pearson_cond)
print(cor_spearman_cond)


library(corrplot)

# Correlación de Spearman
corrplot(cor_spearman_cond,
         method = "color",
         title = "Correlación de Spearman",
         mar = c(0, 0, 2, 0),
         tl.pos = "lt",
         tl.col = "black",
         tl.cex = 0.8,
         addCoef.col = "black",
         number.digits = 2,
         number.cex = 0.8)











#lm(formula, data = ...), formula: especifica qué variable queremos predecir, ormula: especifica qué variable queremos predecir

modelo_lm_variables <- lm(severidad ~ ., data = tabla_completa %>% select(-fecha))
summary(modelo_lm_variables)



