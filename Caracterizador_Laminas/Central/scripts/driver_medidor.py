import pyvisa
import serial
from time import sleep

# Configuración del puerto serie para comunicar con la ESP32
ESP32_SERIAL_PORT = "COM3"  # Cambia esto al puerto correcto donde esté conectada la ESP32
ESP32_BAUDRATE = 115200

def connect_to_pm100d():
    """ Configura el PM100D con parámetros óptimos. """
    rm = pyvisa.ResourceManager('@py')
    instr = rm.open_resource("USB0::4883::32888::P0021181::0::INSTR")
    instr.timeout = 5000  # Timeout de 5 segundos

    # Configuración óptima del medidor
    instr.write("SENS:CORR:WAV 532")      # Longitud de onda: 532 nm
    instr.write("SENS:RANGE:AUTO ON")     # Activar auto-rango
    instr.write("SENS:AVER 500")         # Promediar 2000 mediciones
    instr.write("SENS:POW:UNIT W")        # Configurar unidad en vatios

    return instr

def main():
    """ Toma mediciones solo cuando la ESP32 lo solicite y envía exactamente `numSamples`. """
    instr = None

    try:
        instr = connect_to_pm100d()
        print(" Conexion establecida con el medidor PM100D.")

        # Configurar el puerto serie para la ESP32
        with serial.Serial(ESP32_SERIAL_PORT, ESP32_BAUDRATE, timeout=1) as esp32:
            print(" Comunicacion con la ESP32 lista en", ESP32_SERIAL_PORT)

            while True:
                # Revisar si la ESP32 solicita los datos
                if esp32.in_waiting > 0:
                    sleep(0.01)
                    request = esp32.readline().decode('utf-8').strip()

                    if request.startswith("GET_POWER:"):
                        num_samples = int(request.split(":")[1])  # Extraer número de muestras a tomar
                        print(f" Solicitud recibida: {num_samples} mediciones.")

                        # Tomar exactamente `numSamples` mediciones
                        for _ in range(num_samples):
                            power_value = float(instr.query("MEAS:POW?").strip())  # Leer el PM100D
                            power_microW = f"{power_value * 1e6:.6f}"  # Convertir a µW
                            esp32.write(f"{power_microW}\n".encode('utf-8'))
                            print(f" Enviada a ESP32: {power_microW} µW")
                            #sleep(0.1)  # Pequeño delay entre mediciones para estabilidad

                        print(" Todas las muestras enviadas.")
    
    except Exception as e:
        print(f" Error: {e}")
    finally:
        if instr:
            instr.close()
            print("Conexion con PM100D cerrada.")

if __name__ == "__main__":
    main()
