#include "WavePlateStepper.h"

// Instancia global
WavePlateStepper wavePlateStepper;

// Variables estáticas para manejar las funciones de interrupción
static volatile bool* hallTrigPtr1 = nullptr;
static volatile bool* hallTrigPtr2 = nullptr;

// Constructor
WavePlateStepper::WavePlateStepper() 
    : driver1(HSPI_CS, HSPI_MOSI, HSPI_MISO, HSPI_SCLK),
      driver2(VSPI_CS, VSPI_MOSI, VSPI_MISO, VSPI_SCLK),
      stepper1(AccelStepper::DRIVER, STEP_PIN1, DIR_PIN1),
      stepper2(AccelStepper::DRIVER, STEP_PIN2, DIR_PIN2),
      stepperCurrent1(500),
      stepperCurrent2(500),
      stepperSpeed1(6000),
      stepperSpeed2(6000),
      stepperAcc1(50000),
      stepperAcc2(50000),
      microsteps1(4),
      microsteps2(4),
      hallTriggered1(false),
      hallTriggered2(false),
      sequentialMoveActive1(false),
      sequentialMoveActive2(false),
      sequentialMaxAngle1(0.0),
      sequentialMaxAngle2(0.0),
      sequentialStepAngle1(0.0),
      sequentialStepAngle2(0.0),
      sequentialCurrentAngle1(0.0),
      sequentialCurrentAngle2(0.0),
      webSocket(nullptr)
{
    // Asignar los punteros para las funciones ISR
    hallTrigPtr1 = &hallTriggered1;
    hallTrigPtr2 = &hallTriggered2;
}

void WavePlateStepper::begin(WebSocketsServer* webSocketPtr) {
    webSocket = webSocketPtr;
    
    // Inicializar el driver TMC2130 #1
    driver1.begin();
    driver1.stealthChop(1);
    driver1.rms_current(stepperCurrent1);
    driver1.microsteps(microsteps1);
    
    // Inicializar el driver TMC2130 #2
    driver2.begin();
    driver2.stealthChop(1);
    driver2.rms_current(stepperCurrent2);
    driver2.microsteps(microsteps2);
    
    // Configurar los steppers
    stepper1.setPinsInverted(false, true, false);
    stepper2.setPinsInverted(false, true, false);
    
    // Establecer velocidad y aceleración
    stepper1.setMaxSpeed(stepperSpeed1);
    stepper1.setAcceleration(stepperAcc1);
    stepper2.setMaxSpeed(stepperSpeed2);
    stepper2.setAcceleration(stepperAcc2);
    
    Serial.println("WavePlateStepper iniciado correctamente");
}

// Ejecutar un paso en ambos motores
void WavePlateStepper::run() {
    stepper1.run();
    stepper2.run();
}

// Función ISR para el sensor Hall 1
void IRAM_ATTR WavePlateStepper::hallISR1() {
    if (hallTrigPtr1) {
        *hallTrigPtr1 = true;
    }
}

// Función ISR para el sensor Hall 2
void IRAM_ATTR WavePlateStepper::hallISR2() {
    if (hallTrigPtr2) {
        *hallTrigPtr2 = true;
    }
}

// Obtener ángulo actual del motor 1
float WavePlateStepper::getCurrentAngle1() {
    return -(stepper1.currentPosition() * 360.0) / (SM_RESOLUTION * microsteps1 * GEAR_RATIO);
}

// Obtener ángulo actual del motor 2
float WavePlateStepper::getCurrentAngle2() {
    return -(stepper2.currentPosition() * 360.0) / (SM_RESOLUTION * microsteps2 * GEAR_RATIO);
}

// Mover el motor 1 a un ángulo específico
void WavePlateStepper::moveMotor1ToAngle(float targetAngle) {
    Serial.printf("Moviendo motor 1 a %.2f grados\n", targetAngle);
    
    // Convertir ángulo a pasos
    long steps = -(long)round((targetAngle / 360.0) * (SM_RESOLUTION * microsteps1 * GEAR_RATIO));
    
    // Establecer posición objetivo
    stepper1.moveTo(steps);
    
    // Ejecutar el movimiento
    while (stepper1.distanceToGo() != 0) {
        stepper1.run();
        yield();
    }
    
    // Informar sobre la posición actual
    Serial.printf("Movimiento completado. Posición motor 1: %.2f grados\n", getCurrentAngle1());
}

// Mover el motor 2 a un ángulo específico
void WavePlateStepper::moveMotor2ToAngle(float targetAngle) {
    Serial.printf("Moviendo motor 2 a %.2f grados\n", targetAngle);
    
    // Convertir ángulo a pasos
    long steps = -(long)round((targetAngle / 360.0) * (SM_RESOLUTION * microsteps2 * GEAR_RATIO));
    
    // Establecer posición objetivo
    stepper2.moveTo(steps);
    
    // Ejecutar el movimiento
    while (stepper2.distanceToGo() != 0) {
        stepper2.run();
        yield();
    }
    
    // Informar sobre la posición actual
    Serial.printf("Movimiento completado. Posición motor 2: %.2f grados\n", getCurrentAngle2());
}

// Movimiento secuencial del motor 1
void WavePlateStepper::startSequentialMovement1(float maxAngle, float stepAngle) {
    // Guardamos los ajustes de velocidad actuales
    float prevSpeed = stepper1.maxSpeed();
    float prevAccel = stepper1.acceleration();
    
    // Configuramos velocidad MÁXIMA para el movimiento secuencial
    stepper1.setMaxSpeed(10000); 
    stepper1.setAcceleration(100000); 
    
    // Calculamos el número de pasos totales
    int numSteps = ceil(maxAngle / stepAngle);
    float currentAngle = 0.0;
    
    // Notificar solo al inicio
    if (webSocket) {
        webSocket->broadcastTXT("SEQ_STATUS1:Iniciando secuencia hasta " + String(maxAngle) + "°");
    }
    Serial.println("Iniciando secuencia de movimiento motor 1: " + String(numSteps) + " pasos hasta " + String(maxAngle) + "°");
    
    // Reiniciar posición a 0
    stepper1.setCurrentPosition(0);
    
    // Ejecutar cada paso de forma más eficiente
    for (int step = 1; step <= numSteps; step++) {
        // Calcular el siguiente ángulo objetivo
        currentAngle = step * stepAngle;
        if (currentAngle > maxAngle) currentAngle = maxAngle;
        
        // Convertir ángulo a pasos
        long targetSteps = -round((currentAngle / 360.0) * (SM_RESOLUTION * microsteps1 * GEAR_RATIO));
        
        // Mover al siguiente punto
        stepper1.moveTo(targetSteps);
        
        // Esperar a que complete el movimiento
        while (stepper1.distanceToGo() != 0) {
            stepper1.run();
            yield();
        }
    }
    
    // Restaurar velocidad y aceleración anteriores
    stepper1.setMaxSpeed(prevSpeed);
    stepper1.setAcceleration(prevAccel);
    
    // Notificar solo al finalizar
    if (webSocket) {
        webSocket->broadcastTXT("SEQ_STATUS1:Movimiento secuencial completado hasta " + String(maxAngle) + "°");
    }
    Serial.println("Secuencia motor 1 completada hasta " + String(maxAngle) + "°");
}

// Movimiento secuencial del motor 2
void WavePlateStepper::startSequentialMovement2(float maxAngle, float stepAngle) {
    // Guardamos los ajustes de velocidad actuales
    float prevSpeed = stepper2.maxSpeed();
    float prevAccel = stepper2.acceleration();
    
    // Configuramos velocidad MÁXIMA para el movimiento secuencial
    stepper2.setMaxSpeed(10000); 
    stepper2.setAcceleration(100000); 
    
    // Calculamos el número de pasos totales
    int numSteps = ceil(maxAngle / stepAngle);
    float currentAngle = 0.0;
    
    // Notificar solo al inicio
    if (webSocket) {
        webSocket->broadcastTXT("SEQ_STATUS2:Iniciando secuencia hasta " + String(maxAngle) + "°");
    }
    Serial.println("Iniciando secuencia de movimiento motor 2: " + String(numSteps) + " pasos hasta " + String(maxAngle) + "°");
    
    // Reiniciar posición a 0
    stepper2.setCurrentPosition(0);
    
    // Ejecutar cada paso de forma más eficiente
    for (int step = 1; step <= numSteps; step++) {
        // Calcular el siguiente ángulo objetivo
        currentAngle = step * stepAngle;
        if (currentAngle > maxAngle) currentAngle = maxAngle;
        
        // Convertir ángulo a pasos
        long targetSteps = -round((currentAngle / 360.0) * (SM_RESOLUTION * microsteps2 * GEAR_RATIO));
        
        // Mover al siguiente punto
        stepper2.moveTo(targetSteps);
        
        // Esperar a que complete el movimiento
        while (stepper2.distanceToGo() != 0) {
            stepper2.run();
            yield();
        }
    }
    
    // Restaurar velocidad y aceleración anteriores
    stepper2.setMaxSpeed(prevSpeed);
    stepper2.setAcceleration(prevAccel);
    
    // Notificar solo al finalizar
    if (webSocket) {
        webSocket->broadcastTXT("SEQ_STATUS2:Movimiento secuencial completado hasta " + String(maxAngle) + "°");
    }
    Serial.println("Secuencia motor 2 completada hasta " + String(maxAngle) + "°");
}

// Hacer homing del motor 1
void WavePlateStepper::homeMotor1() {
    Serial.println("Iniciando rutina de homing para motor 1...");
    if (webSocket) {
        webSocket->broadcastTXT("HOMING1_START");
    }

    // Configurar el pin del sensor con pull-up (ya que es de colector abierto)
    pinMode(HALL_SENSOR_PIN1, INPUT);

    // Reiniciar flag de interrupción
    hallTriggered1 = false;

    // Habilitar interrupción en el pin del sensor: se dispara en flanco descendente (cuando el sensor pasa a LOW)
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN1), hallISR1, FALLING);

    // Configurar alta aceleración y velocidad para el giro rápido
    stepper1.setMaxSpeed(5500);
    stepper1.setAcceleration(50000);

    // Calcular el número de pasos correspondientes a 360°
    long stepsFor360 = (long)round((SM_RESOLUTION * microsteps1 * GEAR_RATIO));

    // Iniciar movimiento continuo: dar vuelta completa hasta detectar el imán positivo
    stepper1.moveTo(stepper1.currentPosition() + stepsFor360*3);

    // Giro rápido hasta detectar el imán positivo mediante la interrupción
    while (!hallTriggered1) {
        stepper1.run();
    }

    // Registrar la posición en que se activó el sensor
    long positionAtTrigger = stepper1.currentPosition();

    // Deshabilitar la interrupción para evitar activaciones adicionales
    detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN1));

    // Calcular el número de pasos correspondientes a 345° (suponiendo SM_RESOLUTION * microsteps pasos por revolución)
    long stepsFor345 = (long)round((345.0 / 360.0) * (SM_RESOLUTION * microsteps1 * GEAR_RATIO));

    // Mover 345° adicionales desde el punto de detección
    stepper1.moveTo(positionAtTrigger + stepsFor345);
    while (stepper1.distanceToGo() != 0) {
        stepper1.run();
    }

    Serial.println("Aproximación fina: avanzando paso a paso...");

    // Configurar parámetros para movimiento muy lento y preciso
    stepper1.setMaxSpeed(4000);

    // Avanzar paso a paso encuestando hasta que se active el sensor (cuando el imán positivo esté en posición)
    while (digitalRead(HALL_SENSOR_PIN1) == HIGH) {
        stepper1.moveTo(stepper1.currentPosition() + 1);  // Mover 1 paso
        while (stepper1.distanceToGo() != 0) {
            stepper1.run();
        }
    }

    // Homing completado: establecer la posición actual (en pasos) como 0
    stepper1.setCurrentPosition(0);
    Serial.println("Homing motor 1 completado. Posición 0 establecida.");
    if (webSocket) {
        webSocket->broadcastTXT("HOMING1_COMPLETE");
    }
}

// Hacer homing del motor 2
void WavePlateStepper::homeMotor2() {
    Serial.println("Iniciando rutina de homing para motor 2...");
    if (webSocket) {
        webSocket->broadcastTXT("HOMING2_START");
    }

    // Configurar el pin del sensor con pull-up (ya que es de colector abierto)
    pinMode(HALL_SENSOR_PIN2, INPUT);

    // Reiniciar flag de interrupción
    hallTriggered2 = false;

    // Habilitar interrupción en el pin del sensor: se dispara en flanco descendente (cuando el sensor pasa a LOW)
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN2), hallISR2, FALLING);

    // Configurar alta aceleración y velocidad para el giro rápido
    stepper2.setMaxSpeed(5500);
    stepper2.setAcceleration(50000);

    // Calcular el número de pasos correspondientes a 360°
    long stepsFor360 = (long)round((SM_RESOLUTION * microsteps2 * GEAR_RATIO));

    // Iniciar movimiento continuo: dar vuelta completa hasta detectar el imán positivo
    stepper2.moveTo(stepper2.currentPosition() + stepsFor360*3);

    // Giro rápido hasta detectar el imán positivo mediante la interrupción
    while (!hallTriggered2) {
        stepper2.run();
    }

    // Registrar la posición en que se activó el sensor
    long positionAtTrigger = stepper2.currentPosition();

    // Deshabilitar la interrupción para evitar activaciones adicionales
    detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN2));

    // Calcular el número de pasos correspondientes a 345° (suponiendo SM_RESOLUTION * microsteps pasos por revolución)
    long stepsFor345 = (long)round((345.0 / 360.0) * (SM_RESOLUTION * microsteps2 * GEAR_RATIO));

    // Mover 345° adicionales desde el punto de detección
    stepper2.moveTo(positionAtTrigger + stepsFor345);
    while (stepper2.distanceToGo() != 0) {
        stepper2.run();
    }

    Serial.println("Aproximación fina: avanzando paso a paso...");

    // Configurar parámetros para movimiento muy lento y preciso
    stepper2.setMaxSpeed(4000);

    // Avanzar paso a paso encuestando hasta que se active el sensor (cuando el imán positivo esté en posición)
    while (digitalRead(HALL_SENSOR_PIN2) == HIGH) {
        stepper2.moveTo(stepper2.currentPosition() + 1);  // Mover 1 paso
        while (stepper2.distanceToGo() != 0) {
            stepper2.run();
        }
    }

    // Homing completado: establecer la posición actual (en pasos) como 0
    stepper2.setCurrentPosition(0);
    Serial.println("Homing motor 2 completado. Posición 0 establecida.");
    if (webSocket) {
        webSocket->broadcastTXT("HOMING2_COMPLETE");
    }
}

// Hacer homing de ambos motores
void WavePlateStepper::homeAllMotors() {
    Serial.println("Iniciando rutina de homing para ambos motores...");
    if (webSocket) {
        webSocket->broadcastTXT("HOMING_ALL_START");
    }
  
    homeMotor1();
    homeMotor2();
  
    if (webSocket) {
        webSocket->broadcastTXT("HOMING_ALL_COMPLETE");
    }
    Serial.println("Homing de ambos motores completado.");
}

// Setters para configuración
void WavePlateStepper::setCurrents(int current1, int current2) {
    stepperCurrent1 = current1;
    stepperCurrent2 = current2;
    driver1.rms_current(current1);
    driver2.rms_current(current2);
}

void WavePlateStepper::setSpeeds(int speed1, int speed2) {
    stepperSpeed1 = speed1;
    stepperSpeed2 = speed2;
    stepper1.setMaxSpeed(speed1);
    stepper2.setMaxSpeed(speed2);
}

void WavePlateStepper::setAccelerations(int acc1, int acc2) {
    stepperAcc1 = acc1;
    stepperAcc2 = acc2;
    stepper1.setAcceleration(acc1);
    stepper2.setAcceleration(acc2);
}

void WavePlateStepper::setMicrosteps(int micro1, int micro2) {
    microsteps1 = micro1;
    microsteps2 = micro2;
    driver1.microsteps(micro1);
    driver2.microsteps(micro2);
}
