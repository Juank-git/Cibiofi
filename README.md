# Proyecto BB84 - Criptografía Cuántica

Proyecto de implementación del protocolo BB84 para distribución de claves cuánticas.

## Manual de Usuario

### Guía de Montaje - PCB para Control de Motores Paso a Paso


#### Paso 1: Conexión del regulador y la fuente de alimentación

Primero se conecta el regulador LM2596 **(1)** a la fuente de alimentación externa DC **(2)**. El regulador actúa como un protector: al recibir primero la energía, la transforma y estabiliza antes de que llegue a los componentes sensibles de la placa, evitando posibles daños por voltajes incorrectos.

![Paso 1](docs/images/montaje_paso1.jpg)

#### Paso 2: Ajuste del voltaje de salida del regulador

Con la fuente ya conectada, se verifica que el regulador entregue exactamente 5V:

- Se usa un multímetro en modo voltaje DC **(3)**
- Se colocan sus puntas en los terminales de salida del regulador
- Con un destornillador de pala, se gira el trimmer **(4)** del regulador hasta que el multímetro muestre 5.0V
- **No se debe conectar ningún otro componente hasta completar este ajuste**

[Paso 2](docs/images/montaje_paso2.jpg)

#### Paso 3: Ubicación de componentes

Se instalan los demás elementos respetando cuidadosamente su orientación:

- La **(5) resistencia de 10 kΩ** (en caso de no estar soldada en la placa, su orientación no es relevante)
- El **(6) microcontrolador ESP32-C3 Super Mini**, alineando el conector USB
- El **(7) driver TMC2130**, alineando las 3 ranuras inferiores
- Los **(8) cables del sensor de efecto Hall** (5V, GND, Señal)
- Los **(9) cables hacia los motores paso a paso**, guiandose por los colores indicados sobre la placa.

[Paso 3](docs/images/montaje_paso3.jpg)

#### Ejemplo de montaje completo

![Montaje con componentes ubicados](docs/images/montaje_con_componentes.jpg)

*Imagen de referencia mostrando la PCB con todos los componentes correctamente instalados.*