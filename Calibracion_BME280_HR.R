library(xts)
library(tidyverse)
library(lubridate)
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
library(dplyr)
library(tidyr)
library(naniar) # para lidiar con missing values
library(Metrics)

library(boot)
library(car)
library(QuantPsyc)

attach(Calibración_DHT22) 
names(Calibración_DHT22)
class(Calibración_DHT22$`Tiempo (Min)`)

## _________ Veamos las correlaciones antes de aplicar un modelo 
#Pearson La correlación de Pearson mide la fuerza y dirección de una relación lineal entre dos variables cuantitativas.
cor(`Humedad relativa psicrómetro (%)`,`Humedad relativa sensor barométrico (%)`, method = "pearson")

#Spearman  mide la fuerza y dirección de una relación monotónica entre dos variables
cor(`Humedad relativa psicrómetro (%)`,`Humedad relativa sensor barométrico (%)`, method = "spearman")



#**Si Spearman muestra una correlación alta, pero Pearson no, es probable que la relación sea no lineal. 
#*Esto indica que deberías considerar transformaciones de las variables o un modelo diferente (e.g., regresión no lineal).
#*

# independiente, dependiente
plot(`Humedad relativa sensor barométrico (%)`, `Humedad relativa psicrómetro (%)`)

ggplot(data = Calibración_DHT22, aes(x = `Humedad relativa sensor barométrico (%)`,y = `Humedad relativa psicrómetro (%)`)) +
  
  geom_point(color = 'black', size = 2, alpha = 0.7) +
  
  scale_y_continuous(
    breaks = seq(0, 100, by = 5),  # Controla los puntos de quiebre
    limits = c(45, 90),            # Forzar el rango del eje Y
    expand = c(0, 0)     # Elimina el margen alrededor del eje Y
  ) +
  
  scale_x_continuous(
    breaks = seq(0, 100, by = 5),  # Controla los puntos de quiebre
    limits = c(45, 85),            # Forzar el rango del eje X
    expand = c(0.04, 0)      # Elimina el margen alrededor del eje X
  ) +
  
  xlab('Humedad relativa sensor barométrico (%)') +
  ylab('Humedad relativa psicrómetro (%)') +
  theme_minimal() +
  theme(
    axis.line = element_line(color = "black", linetype = "solid"), 
    axis.ticks = element_line(color = "black", linetype = "solid"),
    panel.grid.major = element_blank(), # Elimina las líneas de cuadrícula principales
    panel.grid.minor = element_blank()  # Elimina las líneas de cuadrícula menores
  )






# Se supone que esto la humedad relativa del psicrometro al hacer la regresion lineal puede explicar las del sensor
# Variable dependiente (respuesta)∼Variable independiente (predictora)
modelo1 = lm(`Humedad relativa psicrómetro (%)`~`Humedad relativa sensor barométrico (%)`,
             data=Calibración_DHT22, na.action = na.exclude) #na.action por si falta alguna observacion // y que no las use

summary(modelo1)

#se utiliza para evaluar la significancia global del modelo y determinar si las variables independientes explican 
#una proporción significativa de la variabilidad de la variable dependiente.
anova(modelo1) # Pero esto no es necesario, se aplica si tengo mas modelos a evaluar o que me aporte algo nuevo



# Calcular valores predichos por el modelo
predicciones <- predict(modelo1, newdata = Calibración_DHT22)
predicciones
#Estadisticos MAPE  mean(abs((data$actual-data$forecast)/data$actual)) * 100
                 # (Valor observado (real), valor predicho )

# Calcular MAPE y RMSE
mape_value <- mape(Calibración_DHT22$`Humedad relativa psicrómetro (%)`, predicciones)
rmse_value <- rmse(Calibración_DHT22$`Humedad relativa psicrómetro (%)`, predicciones)
print(mape_value)
print(rmse_value)

Calibración_DHT22$pred_lower <- predict(modelo1, newdata = Calibración_DHT22, interval = "confidence")[, "lwr"]
Calibración_DHT22$pred_upper <- predict(modelo1, newdata = Calibración_DHT22, interval = "confidence")[, "upr"]

ggplot(data = Calibración_DHT22, aes(x = `Humedad relativa sensor barométrico (%)`,y = `Humedad relativa psicrómetro (%)`)) +
  
  geom_point(color = 'black', size = 2, alpha = 0.7) +
  
  geom_abline(
    intercept = modelo1$coefficients[1], # Intercepto del modelo
    slope = modelo1$coefficients[2],    # Pendiente del modelo
    color = 'blue', linetype = "solid", size = 1) +
  geom_ribbon(aes(ymin = pred_lower, ymax = pred_upper), alpha = 0.2, fill = 'blue') +
  
  scale_y_continuous(
    breaks = seq(0, 100, by = 5),  # Controla los puntos de quiebre
    limits = c(50, 90),            # Forzar el rango del eje Y
    expand = c(0, 0)     # Elimina el margen alrededor del eje Y
  ) +
  
  scale_x_continuous(
    breaks = seq(0, 100, by = 5),  # Controla los puntos de quiebre
    limits = c(48, 85),            # Forzar el rango del eje X
    expand = c(0.01, 0)      # Elimina el margen alrededor del eje X
  ) +
  
  xlab('Humedad relativa sensor barométrico (%)') +
  ylab('Humedad relativa psicrómetro (%)') +
  theme_minimal() +
  theme(
    axis.line = element_line(color = "black", linetype = "solid"), 
    axis.ticks = element_line(color = "black", linetype = "solid"),
    panel.grid.major = element_blank(), # Elimina las líneas de cuadrícula principales
    panel.grid.minor = element_blank()  # Elimina las líneas de cuadrícula menores
  )+
  annotate(
    "text", 
    x = 80, y = 60, # Ajusta las coordenadas según tu rango
    label = paste0("RMSE: ", round(rmse_value, 3), "\nMAPE: ", round(mape_value, 3)), 
    hjust = 1, size = 4, color = "black"
  )







# _________ Pero hay que validar los supuestos del modelo ⤵________________________


# Normalidad 
par(mfrow=c(2,2))
plot(modelo1)
# Primer gráfico
plot(modelo1, which = 1)  # Residuals vs Fitted
mtext("A", side = 3, line = 1, adj = -0.2, cex = 1.5, font = 2)

# Segundo gráfico
plot(modelo1, which = 2)  # Normal Q-Q
mtext("B", side = 3, line = 1, adj = -0.2, cex = 1.5, font = 2)

# Tercer gráfico
plot(modelo1, which = 3)  # Scale-Location
mtext("C", side = 3, line = 1, adj = -0.2, cex = 1.5, font = 2)

# Cuarto gráfico
plot(modelo1, which = 5)  # Residuals vs Leverage
mtext("D", side = 3, line = 1, adj = -0.2, cex = 1.5, font = 2)


par(mfrow=c(1,1))

# 1. # _________ Normalidad de los residuos ⤵________________________

res1=residuals(modelo1)
res1
shapiro.test(res1) # Si el p-valor es menor a 0.05, los residuos no son normales. P = 0.9>0.5

#Permite visualizar la distribución de los residuos. Una forma similar a la campana de Gauss indica normalidad.
hist(res1, col = "salmon")

library(nortest)

# Evaluar la normalidad de los datos
lillie.test(res1) #Lilliefors (Kolmogorov-Smirnov) normality test

# Evaluar si una muestra sigue una distribución específica, en este caso, la normal
cvm.test(res1) # Cramer-von Mises normality test

library(car)
qqPlot(res1) #QQplot de los intervalos de von



# 2. Pruebas para verificar Varianza constante (homocedasticidad) ⤵________________________

# Test de varianza constante 
ncvTest(modelo1) # Supuesto de varianza constante

# plot student residuals vs fitted values 
par(mfrow=c(1,1))
spreadLevelPlot(modelo1) # Gráfico de los valores ajustados vs. residuos estandarizados.
# Si los residuos tienen varianza constante, no debería haber patrones claros en el gráfico.



#intervaloes de confianza opara los valores 

#pendiente he intercepto
confint(modelo1, level = 0.95)



#prediuciones  y confianza para 85
new.dat=data.frame(`Humedad relativa sensor barométrico (%)` = 85)

new.dat

#confianza intervalo para la respuesta
predict(modelo1, new.dat, interval = "confidence")

#predicci[on intervalo para respuesta]
predict(modelo1, new.dat, interval = "prediction")






# Configurar la ventana gráfica para 3 gráficos en una fila
par(mfrow = c(2, 2))  # 1 fila, 3 columnas

# 1. Histograma de los residuos
hist(res1, col = "salmon", main = "Histograma de Residuos")

# 2. QQ-Plot de los residuos
qqPlot(res1, main = "QQ-Plot de Residuos")

# 3. Spread-Level Plot del modelo
spreadLevelPlot(modelo1, main = "Spread-Level Plot")








