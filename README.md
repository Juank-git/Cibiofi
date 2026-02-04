# Repositorio CIBioFi

Este repositorio contiene los recursos para configurar y manejar placas de control de motores paso a paso, donde cada placa controla un motor individual con precisión. Su desarrollo está orientado principalmente a la automatización de sistemas que requieren posicionamiento rápidi y exacto, incluyendo aplicaciones en criptografía cuántica como la implementación del protocolo BB84 y otros experimentos de óptica cuántica.

## Manual de Usuario

### Guía de Montaje - PCB para Control de Motores Paso a Paso


#### Paso 1: Conexión del regulador y la fuente de alimentación

Primero se conecta el regulador LM2596 **(1)** y luego la fuente de alimentación externa DC **(2)**. El regulador fija el voltaje de entrada en un valor estable y seguro, el cual sirve de alimentación para los demás componentes del circuito.

![Paso 1](docs/images/montaje_paso1.jpg)

#### Paso 2: Ajuste del voltaje de salida del regulador

Con la fuente ya conectada, se verifica que el regulador entregue exactamente 5V:

- Se usa un multímetro en modo voltaje DC **(3)**
- Se colocan sus puntas en los terminales de salida del regulador, la punta roja en el terminal positivo (OUT+) y la punta negra en el terminal negativo (OUT-)
- Con un destornillador de pala, se gira el trimmer **(4)** del regulador hasta que el multímetro muestre 5.0V
- **No se debe conectar ningún otro componente hasta completar este ajuste**

[Paso 2](docs/images/montaje_paso2.jpg)

#### Paso 3: Ubicación de componentes

Una vez verificado el voltaje de salida del regulador se ubican los demás componentes respetando cuidadosamente su orientación:

- La **(5) resistencia de 10 kΩ** (en caso de no estar soldada en la placa), su orientación no es relevante
- El **(6) microcontrolador ESP32-C3 Super Mini**, alineando el conector USB
- El **(7) driver TMC2130**, alineando las 3 ranuras inferiores
- Los **(8) cables del sensor de efecto Hall** (5V, GND, Señal)
- Los **(9) cables hacia los motores paso a paso**, guiandose por los colores indicados sobre la placa.

[Paso 3](docs/images/montaje_paso3.jpg)

#### Ejemplo de montaje completo

El montaje completo, sin los cables del motor y del sensor, debe verse de la siguiente manera:

![Montaje con componentes ubicados](docs/images/montaje_con_componentes.jpg)

*Imagen de referencia mostrando la PCB con los componentes correctamente ubicados.*