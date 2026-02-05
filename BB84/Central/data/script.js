const socket = new WebSocket(`ws://${window.location.hostname}:81/`);
let pulsoActual = 1;

// Datos para el histograma
let histogramData = {};

let rawDataDetector0 = [];
let rawDataDetector1 = [];

// Arrays para almacenar el historial completo de datos
const completeDataHistory = {
    pulsos: [],
    detector0: [],
    detector1: [],
    baseAlice: [],
    bitEnviado: [],
    baseBob: [],
    bitRecibido: []
};

// Función para cambiar entre pestañas - actualizada para las nuevas pestañas
function openTab(tabName) {
    const tabContents = document.getElementsByClassName('tab-content');
    for (let i = 0; i < tabContents.length; i++) {
        tabContents[i].classList.remove('active');
    }
    
    const tabButtons = document.getElementsByClassName('nav-tab');
    for (let i = 0; i < tabButtons.length; i++) {
        tabButtons[i].classList.remove('active');
    }
    
    document.getElementById(tabName).classList.add('active');
    
    // Encontrar y activar el botón correspondiente
    for (let i = 0; i < tabButtons.length; i++) {
        if (tabButtons[i].getAttribute('onclick').includes(tabName)) {
            tabButtons[i].classList.add('active');
            break;
        }
    }
}

// Modificar el manejador de mensajes del WebSocket para eliminar motor
socket.onmessage = function(event) {
    try {
        // Intentar parsear como JSON primero
        try {
            const data = JSON.parse(event.data);
            
            if (data.status === "ok") {
                document.getElementById("status-message").textContent = data.message;
                // Solo reiniciamos los datos si NO es un mensaje de abortar
                if (data.message !== "Protocolo abortado.") {
                    pulsoActual = 1;
                    rawDataDetector0 = [];
                    rawDataDetector1 = [];
                    histogramData = {};
                    histograma.data.labels = [];
                    histograma.data.datasets[0].data = [];
                    histograma.data.datasets[1].data = [];
                    histograma.update();
                    
                    timelineChart.data.labels = [];
                    timelineChart.data.datasets[0].data = [];
                    timelineChart.data.datasets[1].data = [];
                    timelineChart.update();

                    // También limpiamos la tabla solo al iniciar nuevo protocolo
                    document.getElementById("datos-cuerpo").innerHTML = '';
                    
                    // Limpiar historial completo
                    completeDataHistory.pulsos = [];
                    completeDataHistory.detector0 = [];
                    completeDataHistory.detector1 = [];
                    completeDataHistory.baseAlice = [];
                    completeDataHistory.bitEnviado = [];
                    completeDataHistory.baseBob = [];
                    completeDataHistory.bitRecibido = [];
                    
                    // Reiniciar estadísticas
                    actualizarEstadisticas();
                }
            } else if (data.conteos) {
                // Conversion de valores numéricos a símbolos
                const baseAliceSymbol = data.baseAlice === 0 ? "+" : "x";
                const baseBobSymbol = data.baseBob === 0 ? "+" : "x";
                const bitEnviado = data.bitEnviado !== undefined ? String(data.bitEnviado) : "-";
                const bitRecibido = data.bitRecibido !== undefined ? String(data.bitRecibido) : "-";
                
                // Actualizar tabla con el nuevo orden de columnas
                const tablaCuerpo = document.getElementById("datos-cuerpo");
                const fila = document.createElement("tr");
                
                // Verificar si las bases coinciden
                const basesCoinciden = baseAliceSymbol === baseBobSymbol;
                
                // Aplicar clase según coincidencia de bases y bits
                if (basesCoinciden) {
                    if (bitEnviado !== "-" && bitRecibido !== "-" && bitEnviado !== bitRecibido) {
                        // Bases coinciden pero bits no coinciden (error)
                        fila.classList.add('bits-error');
                    } else {
                        // Bases coinciden y bits también (o no están definidos)
                        fila.classList.add('bases-match');
                    }
                }

                fila.innerHTML = `
                    <td>${pulsoActual}</td>
                    <td>${baseAliceSymbol}</td>
                    <td>${baseBobSymbol}</td>
                    <td>${bitEnviado}</td>
                    <td>${bitRecibido}</td>
                    <td>${data.conteos.detector0}</td>
                    <td>${data.conteos.detector1}</td>
                `;
                tablaCuerpo.appendChild(fila);

                // Almacenar en el historial completo
                completeDataHistory.pulsos.push(pulsoActual);
                completeDataHistory.detector0.push(data.conteos.detector0);
                completeDataHistory.detector1.push(data.conteos.detector1);
                completeDataHistory.baseAlice.push(baseAliceSymbol);
                completeDataHistory.bitEnviado.push(bitEnviado);
                completeDataHistory.baseBob.push(baseBobSymbol);
                completeDataHistory.bitRecibido.push(bitRecibido);

                rawDataDetector0.push(data.conteos.detector0);
                rawDataDetector1.push(data.conteos.detector1);
                
                // Llamar a la función para actualizar las estadísticas
                actualizarEstadisticas();

                // Actualizar histograma
                actualizarHistograma(data.conteos.detector0, data.conteos.detector1);

                // Actualizar el gráfico de línea temporal
                timelineChart.data.labels.push(pulsoActual);
                timelineChart.data.datasets[0].data.push(data.conteos.detector0);
                timelineChart.data.datasets[1].data.push(data.conteos.detector1);
                timelineChart.update();

                // Actualizar los datos en la nueva sección de visualización
                document.getElementById("last-pulse").textContent = pulsoActual;
                document.getElementById("last-detector0").textContent = data.conteos.detector0;
                document.getElementById("last-detector1").textContent = data.conteos.detector1;

                pulsoActual++;
            }
        } catch (jsonError) {
            // No necesitamos procesar mensajes no-JSON 
            // ya que estaban relacionados con el motor
            console.log("Mensaje no JSON recibido:", event.data);
        }
    } catch (error) {
        console.error("Error al procesar mensaje:", error, event.data);
    }
};

// Configuración del histograma
const histogramaConfig = {
    type: 'bar',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Detector 0',
                data: [],
                backgroundColor: 'rgba(79, 172, 254, 0.7)',
                borderColor: 'rgba(79, 172, 254, 1)',
                borderWidth: 1
            },
            {
                label: 'Detector 1',
                data: [],
                backgroundColor: 'rgba(0, 242, 254, 0.7)',
                borderColor: 'rgba(0, 242, 254, 1)',
                borderWidth: 1
            }
        ]
    },
    options: {
        responsive: true,
        scales: {
            y: {
                beginAtZero: true,
                stacked: true,
                title: {
                    display: true,
                    text: 'Frecuencia'
                }
            },
            x: {
                stacked: true,
                title: {
                    display: true,
                    text: 'Cantidad de fotones'
                }
            }
        },
        plugins: {
            title: {
                display: true,
                text: 'Histograma de Detecciones'
            }
        }
    }
};

// Crear el histograma
const ctx = document.getElementById('histogramaCombinad').getContext('2d');
const histograma = new Chart(ctx, histogramaConfig);

// Configuración del gráfico de línea temporal
const timelineConfig = {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Detector 0',
                data: [],
                borderColor: 'rgba(79, 172, 254, 1)',
                tension: 0.3,
                fill: false
            },
            {
                label: 'Detector 1',
                data: [],
                borderColor: 'rgba(0, 242, 254, 1)',
                tension: 0.3,
                fill: false
            }
        ]
    },
    options: {
        responsive: true,
        scales: {
            y: {
                beginAtZero: true,
                title: {
                    display: true,
                    text: 'Cantidad de detecciones'
                }
            },
            x: {
                title: {
                    display: true,
                    text: 'Número de pulso'
                }
            }
        },
        plugins: {
            title: {
                display: true,
                text: 'Detecciones por pulso'
            }
        }
    }
};

// Crear el gráfico de línea temporal
const timelineCtx = document.getElementById('timelineChart').getContext('2d');
const timelineChart = new Chart(timelineCtx, timelineConfig);

function calcularEstadisticas(datos) {
    const n = datos.length;
    const mean = datos.reduce((a, b) => a + b, 0) / n;
    const median = datos.sort((a, b) => a - b)[Math.floor(n / 2)];
    const mode = datos.sort((a,b) =>
            datos.filter(v => v===a).length
        - datos.filter(v => v===b).length
    ).pop();
    const variance = datos.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / n;
    const stddev = Math.sqrt(variance);
    const cv = stddev / mean;

    return { mean, median, mode, variance, stddev, cv };
}

// ...existing code...
function actualizarEstadisticas() {
    // Contadores para estadísticas por polarización
    let sentH = 0, sentV = 0, sentD = 0, sentA = 0;
    let receivedH = 0, receivedV = 0, receivedD = 0, receivedA = 0;
    let errorsH = 0, errorsV = 0, errorsD = 0, errorsA = 0;
    let totalErrors = 0;
    let totalMeasurements = 0;
    
    // Contadores para fotones promedio por polarización
    let photonSumH = 0, photonCountH = 0;
    let photonSumV = 0, photonCountV = 0;
    let photonSumD = 0, photonCountD = 0;
    let photonSumA = 0, photonCountA = 0;
    
    // Iterar sobre todos los datos recopilados
    for (let i = 0; i < completeDataHistory.bitEnviado.length; i++) {
        // Solo considerar entradas donde las bases de Alice y Bob coincidan
        if (completeDataHistory.baseAlice[i] === completeDataHistory.baseBob[i] && 
            completeDataHistory.bitEnviado[i] !== "-" && 
            completeDataHistory.bitRecibido[i] !== "-") {
            
            totalMeasurements++;
            const baseUsada = completeDataHistory.baseAlice[i]; // + o x
            const bitEnviado = completeDataHistory.bitEnviado[i]; // 0 o 1
            const bitRecibido = completeDataHistory.bitRecibido[i]; // 0 o 1
            const totalPhotons = completeDataHistory.detector0[i] + completeDataHistory.detector1[i];
            
            // Determinar qué polarización se usó basado en la base y el bit enviado
            if (baseUsada === '+') {
                if (bitEnviado === '0') { // Polarización H
                    sentH++;
                    if (bitRecibido !== '0') {
                        errorsH++;
                        totalErrors++;
                    }
                } else if (bitEnviado === '1') { // Polarización V
                    sentV++;
                    if (bitRecibido !== '1') {
                        errorsV++;
                        totalErrors++;
                    }
                }
            } else if (baseUsada === 'x') {
                if (bitEnviado === '0') { // Polarización D
                    sentD++;
                    if (bitRecibido !== '0') {
                        errorsD++;
                        totalErrors++;
                    }
                } else if (bitEnviado === '1') { // Polarización A
                    sentA++;
                    if (bitRecibido !== '1') {
                        errorsA++;
                        totalErrors++;
                    }
                }
            }
            
            // Contar bits recibidos y acumular fotones por polarización
            if (baseUsada === '+') {
                if (bitRecibido === '0') { // Recibido como H
                    receivedH++;
                    photonSumH += totalPhotons;
                    photonCountH++;
                } else if (bitRecibido === '1') { // Recibido como V
                    receivedV++;
                    photonSumV += totalPhotons;
                    photonCountV++;
                }
            } else if (baseUsada === 'x') {
                if (bitRecibido === '0') { // Recibido como D
                    receivedD++;
                    photonSumD += totalPhotons;
                    photonCountD++;
                } else if (bitRecibido === '1') { // Recibido como A
                    receivedA++;
                    photonSumA += totalPhotons;
                    photonCountA++;
                }
            }
        }
    }
    
    // Calcular QER para cada polarización
    const qerH = sentH > 0 ? (errorsH / sentH) : 0;
    const qerV = sentV > 0 ? (errorsV / sentV) : 0;
    const qerD = sentD > 0 ? (errorsD / sentD) : 0;
    const qerA = sentA > 0 ? (errorsA / sentA) : 0;
    const globalQER = totalMeasurements > 0 ? (totalErrors / totalMeasurements) : 0;

    // Calcular promedios de fotones
    const avgPhotonsH = photonCountH > 0 ? (photonSumH / photonCountH).toFixed(2) : "-";
    const avgPhotonsV = photonCountV > 0 ? (photonSumV / photonCountV).toFixed(2) : "-";
    const avgPhotonsD = photonCountD > 0 ? (photonSumD / photonCountD).toFixed(2) : "-";
    const avgPhotonsA = photonCountA > 0 ? (photonSumA / photonCountA).toFixed(2) : "-";
    
    // Calcular precisión (1 - QER)
    const accuracyH = 1 - qerH;
    const accuracyV = 1 - qerV;
    const accuracyD = 1 - qerD;
    const accuracyA = 1 - qerA;
    const globalAccuracy = 1 - globalQER;
    
    // Actualizar la interfaz con las estadísticas
    // Polarización H
    document.getElementById("sent-H").textContent = sentH;
    document.getElementById("received-H").textContent = receivedH;
    document.getElementById("qer-H").textContent = (qerH * 100).toFixed(2) + "%";
    document.getElementById("accuracy-H").textContent = (accuracyH * 100).toFixed(2) + "%";
    document.getElementById("avg-photons-H").textContent = avgPhotonsH;
    
    // Polarización V
    document.getElementById("sent-V").textContent = sentV;
    document.getElementById("received-V").textContent = receivedV;
    document.getElementById("qer-V").textContent = (qerV * 100).toFixed(2) + "%";
    document.getElementById("accuracy-V").textContent = (accuracyV * 100).toFixed(2) + "%";
    document.getElementById("avg-photons-V").textContent = avgPhotonsV;
    
    // Polarización D
    document.getElementById("sent-D").textContent = sentD;
    document.getElementById("received-D").textContent = receivedD;
    document.getElementById("qer-D").textContent = (qerD * 100).toFixed(2) + "%";
    document.getElementById("accuracy-D").textContent = (accuracyD * 100).toFixed(2) + "%";
    document.getElementById("avg-photons-D").textContent = avgPhotonsD;
    
    // Polarización A
    document.getElementById("sent-A").textContent = sentA;
    document.getElementById("received-A").textContent = receivedA;
    document.getElementById("qer-A").textContent = (qerA * 100).toFixed(2) + "%";
    document.getElementById("accuracy-A").textContent = (accuracyA * 100).toFixed(2) + "%";
    document.getElementById("avg-photons-A").textContent = avgPhotonsA;
    
    // Global
    document.getElementById("total-measurements").textContent = totalMeasurements;
    document.getElementById("correct-measurements").textContent = totalMeasurements - totalErrors;
    document.getElementById("global-qer").textContent = (globalQER * 100).toFixed(2) + "%";
    document.getElementById("global-accuracy").textContent = (globalAccuracy * 100).toFixed(2) + "%";
}
// ...existing code...

// Función para actualizar el histograma
function actualizarHistograma(datos0, datos1) {
    // Actualizar datos del histograma para detector 0
    if (datos0 in histogramData) {
        if (!histogramData[datos0].detector0) histogramData[datos0].detector0 = 0;
        histogramData[datos0].detector0++;
    } else {
        histogramData[datos0] = { detector0: 1, detector1: 0 };
    }

    // Actualizar datos del histograma para detector 1
    if (datos1 in histogramData) {
        if (!histogramData[datos1].detector1) histogramData[datos1].detector1 = 0;
        histogramData[datos1].detector1++;
    } else {
        histogramData[datos1] = { detector0: 0, detector1: 1 };
    }

    // Convertir datos a arrays para Chart.js
    const labels = Object.keys(histogramData).sort((a, b) => Number(a) - Number(b));
    const values0 = labels.map(key => histogramData[key].detector0 || 0);
    const values1 = labels.map(key => histogramData[key].detector1 || 0);

    // Actualizar el gráfico
    histograma.data.labels = labels;
    histograma.data.datasets[0].data = values0;
    histograma.data.datasets[1].data = values1;
    histograma.update();
}

function cambiarGrafico(tipo) {
    document.querySelectorAll('.tab-button').forEach(button => {
        button.classList.remove('active');
    });
    document.querySelectorAll('.chart-wrapper').forEach(wrapper => {
        wrapper.classList.remove('active');
    });

    if (tipo === 'histograma') {
        document.querySelector('.tab-button:first-child').classList.add('active');
        document.getElementById('histograma-wrapper').classList.add('active');
    } else {
        document.querySelector('.tab-button:last-child').classList.add('active');
        document.getElementById('timeline-wrapper').classList.add('active');
    }
}

function enviarConfiguracion() {
    const num_pulsos = document.getElementById('num_pulsos').value;
    const duracion_value = document.getElementById('duracion_us').value;
    const duracion_unit = document.getElementById('duracion_unit').value;

    // Convertir a microsegundos según la unidad seleccionada
    let duracion_us = parseFloat(duracion_value);

    if (duracion_unit === 'ms') {
        duracion_us *= 1000;
    } else if (duracion_unit === 's') {
        duracion_us *= 1000000;
    }

    // Validar los rangos en microsegundos
    duracion_us = Math.round(duracion_us);

    if (duracion_us < 0 || duracion_us > 16777215) {
        document.getElementById("status-message").textContent = 
            "Error: La duración ON debe estar entre 0 y 16777215 μs";
        return;
    }

    // Limpiar todo al iniciar nuevo experimento
    document.getElementById("datos-cuerpo").innerHTML = '';
    pulsoActual = 1;
    rawDataDetector0 = [];
    rawDataDetector1 = [];
    histogramData = {};
    histograma.data.labels = [];
    histograma.data.datasets[0].data = [];
    histograma.data.datasets[1].data = [];
    histograma.update();
    
    timelineChart.data.labels = [];
    timelineChart.data.datasets[0].data = [];
    timelineChart.data.datasets[1].data = [];
    timelineChart.update();

    const configuracion = JSON.stringify({
        num_pulsos: parseInt(num_pulsos, 10),
        duracion_us: duracion_us
    });

    socket.send(configuracion);
    document.getElementById("status-message").textContent = "Configuración enviada correctamente.";
}

function abortarProtocolo() {
    socket.send("abort");
    document.getElementById("status-message").textContent = "Abortando protocolo...";
}

function ejecutarHoming() {
    socket.send("HOMING_ALL");
    document.getElementById("status-message").textContent = "Ejecutando homing en Alice y Bob...";
}

// ============================================
// FUNCIONES DE CONTROL MANUAL
// ============================================

function moverAliceAngulo() {
    const angle = parseFloat(document.getElementById('alice-angle').value);
    if (isNaN(angle) || angle < 0 || angle > 360) {
        alert("Por favor, introduce un ángulo válido entre 0 y 360 grados");
        return;
    }
    
    const command = JSON.stringify({
        type: "MOVE_ALICE",
        angle: angle
    });
    
    socket.send(command);
    document.getElementById('alice-status').textContent = `Moviendo a ${angle}°...`;
}

function moverAlicePreset(base, bit) {
    const angles = [
        [47.7, 2.7],   // Base 0: [H, V]
        [25.2, 70.2]   // Base 1: [D, A]
    ];
    
    const angle = angles[base][bit];
    const labels = [
        ["H (Horizontal)", "V (Vertical)"],
        ["D (Diagonal)", "A (Antidiagonal)"]
    ];
    
    const command = JSON.stringify({
        type: "MOVE_ALICE",
        angle: angle
    });
    
    socket.send(command);
    document.getElementById('alice-status').textContent = `Moviendo a ${labels[base][bit]} - ${angle}°...`;
}

function moverBobAngulo() {
    const angle = parseFloat(document.getElementById('bob-angle').value);
    if (isNaN(angle) || angle < 0 || angle > 360) {
        alert("Por favor, introduce un ángulo válido entre 0 y 360 grados");
        return;
    }
    
    const command = JSON.stringify({
        type: "MOVE_BOB",
        angle: angle
    });
    
    socket.send(command);
    document.getElementById('bob-status').textContent = `Moviendo a ${angle}°...`;
}

function moverBobPreset(base) {
    const angles = [13.95, 36.45];  // [Base +, Base x]
    const labels = ["Base + (Rectilinea)", "Base x (Diagonal)"];
    
    const angle = angles[base];
    
    const command = JSON.stringify({
        type: "MOVE_BOB",
        angle: angle
    });
    
    socket.send(command);
    document.getElementById('bob-status').textContent = `Moviendo a ${labels[base]} - ${angle}°...`;
}

// Actualizar rangos del input de duración según la unidad seleccionada
function updateDurationRanges() {
    const unit = document.getElementById('duracion_unit').value;
    const input = document.getElementById('duracion_us');
    const rangeText = document.getElementById('duracion_range');
    
    if (unit === 'us') {
        input.max = 16777215;
        rangeText.textContent = 'Rango: 0 - 16777215 μs';
    } else if (unit === 'ms') {
        input.max = 16777;
        rangeText.textContent = 'Rango: 0 - 16777 ms (16777215 μs)';
    } else if (unit === 's') {
        input.max = 16;
        rangeText.textContent = 'Rango: 0 - 16 s (16777215 μs)';
    }
}

// Función para descargar los datos como CSV
function downloadCSV() {
    let csv = "Pulso,BaseAlice,BaseBob,BitEnviado,BitRecibido,Detector0,Detector1\n";
    
    for (let i = 0; i < completeDataHistory.pulsos.length; i++) {
        csv += `${completeDataHistory.pulsos[i]},`;
        csv += `${completeDataHistory.baseAlice[i]},`;
        csv += `${completeDataHistory.baseBob[i]},`;
        csv += `${completeDataHistory.bitEnviado[i]},`;
        csv += `${completeDataHistory.bitRecibido[i]},`;
        csv += `${completeDataHistory.detector0[i]},`;
        csv += `${completeDataHistory.detector1[i]}\n`;
    }

    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `Datos_Deteccion_${timestamp}.csv`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    window.URL.revokeObjectURL(url);
    
    alert("✅ Archivo CSV descargado con " + completeDataHistory.pulsos.length + " registros.");
}

// Función para actualizar los rangos de la duración ON según la unidad seleccionada
function updateDurationRanges() {
    const unitSelect = document.getElementById('duracion_unit');
    const inputField = document.getElementById('duracion_us');
    const rangeText = document.getElementById('duracion_range');
    const unit = unitSelect.value;
    
    if (unit === 'us') {
        inputField.min = 0;
        inputField.max = 16777215;
        rangeText.textContent = "Rango: 0 - 16777215 μs";
    } else if (unit === 'ms') {
        inputField.min = 0;
        inputField.max = 16777.215;
        rangeText.textContent = "Rango: 0 - 16777.215 ms";
    } else if (unit === 's') {
        inputField.min = 0;
        inputField.max = 16.777215;
        rangeText.textContent = "Rango: 0 - 16.777215 s";
    }
}

// Inicializar rangos al cargar la página
document.addEventListener('DOMContentLoaded', function() {
    updateDurationRanges();
});

// Variables para almacenar los bits procesados en cada paso del BB84
let bitsFiltrados = [];
let bitsValidacion = [];
let bitsClave = [];
let claveAliceHex = "";
let claveBobHex = "";

/**
 * Función principal para ejecutar el protocolo BB84 y generar la clave final
 */
function generarClaveBB84() {
    // Verificar si hay suficientes datos para proceder
    if (completeDataHistory.pulsos.length === 0) {
        alert("⚠️ No hay datos para generar clave. Por favor, ejecute un experimento primero.");
        return;
    }
    
    // Paso 1: Filtrar bits con bases coincidentes
    filtrarBaseCoincidente();
    
    // Paso 2: Dividir en serie de validación y clave
    dividirSeries();
    
    // Paso 3: Validar y detectar intrusiones
    validarTransmision();
    
    // Paso 4: Generar clave final
    generarClaveFinal();
    
    // Asegurar que el contador se actualice después de generar la clave
    setTimeout(actualizarContadorOriginal, 500);
}

/**
 * Paso 1: Filtrar únicamente los bits donde las bases de Alice y Bob coinciden
 * y el bit enviado y recibido están definidos (no son "-")
 */
function filtrarBaseCoincidente() {
    bitsFiltrados = [];
    
    for (let i = 0; i < completeDataHistory.baseAlice.length; i++) {
        if (completeDataHistory.baseAlice[i] === completeDataHistory.baseBob[i] &&
            completeDataHistory.bitEnviado[i] !== "-" &&
            completeDataHistory.bitRecibido[i] !== "-") {
            
            bitsFiltrados.push({
                pulso: completeDataHistory.pulsos[i],
                base: completeDataHistory.baseAlice[i],
                bitEnviado: completeDataHistory.bitEnviado[i],
                bitRecibido: completeDataHistory.bitRecibido[i]
            });
        }
    }
    
    // Actualizar estadísticas en la interfaz
    const totalBits = completeDataHistory.pulsos.length;
    const matchingBits = bitsFiltrados.length;
    const efficiency = totalBits > 0 ? (matchingBits / totalBits * 100).toFixed(2) : "0";
    
    document.getElementById("total-bits").textContent = totalBits;
    document.getElementById("matching-bases-bits").textContent = matchingBits;
    document.getElementById("matching-efficiency").textContent = efficiency + "%";
    
    // Visualización de bits
    visualizarBits("bit-visualization-1", bitsFiltrados.map(bit => bit.bitRecibido));
}

/**
 * Paso 2: Dividir los bits filtrados en serie de validación y serie para clave final
 * Para simplicidad, usamos 50% para validación y 50% para la clave final
 */
function dividirSeries() {
    // Determinar punto medio para división (50% para validación, 50% para clave)
    const mitad = Math.floor(bitsFiltrados.length / 2);
    
    // Dividir los bits
    bitsValidacion = bitsFiltrados.slice(0, mitad);
    bitsClave = bitsFiltrados.slice(mitad);
    
    // Actualizar estadísticas en la interfaz
    document.getElementById("validation-bits-count").textContent = bitsValidacion.length;
    document.getElementById("key-bits-count").textContent = bitsClave.length;
    
    // Visualización de series
    visualizarBits("validation-bits", bitsValidacion.map(bit => bit.bitRecibido));
    visualizarBits("key-bits", bitsClave.map(bit => bit.bitRecibido));
}

/**
 * Paso 3: Validar la transmisión comparando los bits enviados con los recibidos
 * en la serie de validación para detectar intrusiones (QBER < 15% = seguro)
 */
function validarTransmision() {
    let errores = 0;
    
    // Contar errores en la serie de validación
    for (let i = 0; i < bitsValidacion.length; i++) {
        if (bitsValidacion[i].bitEnviado !== bitsValidacion[i].bitRecibido) {
            errores++;
        }
    }
    
    // Calcular QBER (Quantum Bit Error Rate)
    const qber = bitsValidacion.length > 0 ? (errores / bitsValidacion.length * 100).toFixed(2) : "0";
    const isSecure = parseFloat(qber) < 15;
    
    // Actualizar estadísticas en la interfaz
    document.getElementById("validation-errors").textContent = errores;
    document.getElementById("qber-value").textContent = qber + "%";
    
    // Mostrar estado de seguridad
    const statusElement = document.getElementById("transmission-status");
    if (isSecure) {
        statusElement.textContent = "✅ SEGURA - No se detectaron intrusos";
        statusElement.style.color = "#4caf50"; // Verde
    } else {
        statusElement.textContent = "⚠️ COMPROMETIDA - Posible intruso detectado";
        statusElement.style.color = "#f44336"; // Rojo
    }
}

/**
 * Paso 4: Generar las claves finales de Alice y Bob usando los bits destinados para la clave
 * y convertirlas a formato hexadecimal
 */
function generarClaveFinal() {
    // Extraer bits de Alice (enviados) y Bob (recibidos) para la clave final
    const bitsAlice = bitsClave.map(bit => bit.bitEnviado);
    const bitsBob = bitsClave.map(bit => bit.bitRecibido);
    
    // Contar diferencias entre las claves
    let diferencias = 0;
    for (let i = 0; i < bitsAlice.length; i++) {
        if (bitsAlice[i] !== bitsBob[i]) {
            diferencias++;
        }
    }
    
    // Calcular porcentaje de coincidencia
    const porcentajeCoincidencia = bitsAlice.length > 0 
        ? (100 - (diferencias / bitsAlice.length * 100)).toFixed(2) 
        : "0";
    
    // Convertir a hexadecimal
    claveAliceHex = bitsAHexadecimal(bitsAlice);
    claveBobHex = bitsAHexadecimal(bitsBob);
    
    // Actualizar la interfaz
    document.getElementById("key-length").textContent = bitsAlice.length;
    document.getElementById("key-differences").textContent = diferencias;
    document.getElementById("key-match-percentage").textContent = porcentajeCoincidencia + "%";
    
    // Limpiar los contenedores antes de visualizar
    const containerAlice = document.getElementById("alice-key-bits");
    const containerBob = document.getElementById("bob-key-bits");
    containerAlice.innerHTML = "";
    containerBob.innerHTML = "";
    
    // Crear y añadir los elementos de bit para Alice y Bob al mismo tiempo
    for (let i = 0; i < bitsAlice.length; i++) {
        // Bit de Alice
        const bitElementAlice = document.createElement("span");
        bitElementAlice.className = "bit-box " + (bitsAlice[i] === "1" ? "bit-one" : "bit-zero");
        bitElementAlice.textContent = bitsAlice[i];
        containerAlice.appendChild(bitElementAlice);
        
        // Bit de Bob
        const bitElementBob = document.createElement("span");
        bitElementBob.className = "bit-box " + (bitsBob[i] === "1" ? "bit-one" : "bit-zero");
        bitElementBob.textContent = bitsBob[i];
        containerBob.appendChild(bitElementBob);
        
        // Si hay diferencia, marcar ambos bits
        if (bitsAlice[i] !== bitsBob[i]) {
            bitElementAlice.classList.add('bit-different');
            bitElementBob.classList.add('bit-different');
        }
    }
    
    // Mostrar claves hexadecimales
    document.getElementById("alice-key-hex").textContent = claveAliceHex;
    document.getElementById("bob-key-hex").textContent = claveBobHex;
    
    // Actualizar el contador de caracteres para la demostración de encriptación
    // Llamamos explícitamente a la función después de un pequeño retraso
    setTimeout(function() {
        actualizarContadorOriginal();
        
        // Hacer scroll a la sección de demostración para que sea visible
        const demoSection = document.getElementById('step5-container');
        if (demoSection) {
            demoSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }
    }, 300);
}

/**
 * Función para resaltar visualmente las diferencias entre las claves de Alice y Bob
 */
function resaltarDiferenciasBits(bitsAlice, bitsBob) {
    const contAlice = document.getElementById("alice-key-bits");
    const contBob = document.getElementById("bob-key-bits");
    
    // Obtener todos los elementos de bit
    const bitsElementsAlice = contAlice.querySelectorAll('.bit-box');
    const bitsElementsBob = contBob.querySelectorAll('.bit-box');
    
    // Marcar diferencias
    for (let i = 0; i < bitsAlice.length; i++) {
        if (bitsAlice[i] !== bitsBob[i]) {
            bitsElementsAlice[i].classList.add('bit-different');
            bitsElementsBob[i].classList.add('bit-different');
        }
    }
}

/**
 * Función para copiar la clave hexadecimal al portapapeles
 */
function copiarClaveAlPortapapeles(usuario) {
    const clave = usuario === 'alice' ? claveAliceHex : claveBobHex;
    
    if (!clave) {
        alert(`⚠️ No hay clave de ${usuario === 'alice' ? 'Alice' : 'Bob'} generada para copiar.`);
        return;
    }
    
    navigator.clipboard.writeText(clave).then(() => {
        alert(`✅ Clave de ${usuario === 'alice' ? 'Alice' : 'Bob'} copiada al portapapeles: ${clave}`);
    }).catch(err => {
        console.error('Error al copiar: ', err);
        alert("❌ Error al copiar la clave. Intente nuevamente.");
    });
}

/**
 * Función para guardar las claves como archivo de texto
 */
function guardarClaveComoArchivo() {
    if (!claveAliceHex || !claveBobHex) {
        alert("⚠️ No hay claves generadas para guardar.");
        return;
    }
    
    const contenido = `CLAVES BB84 GENERADAS
Fecha: ${new Date().toLocaleString()}
Longitud: ${document.getElementById("key-length").textContent} bits
QBER: ${document.getElementById("qber-value").textContent}
Estado: ${document.getElementById("transmission-status").textContent}
Diferencias entre claves: ${document.getElementById("key-differences").textContent} bits
Porcentaje de coincidencia: ${document.getElementById("key-match-percentage").textContent}

CLAVE DE ALICE (HEX): ${claveAliceHex}
CLAVE DE BOB (HEX): ${claveBobHex}

CLAVE DE ALICE (BITS): ${bitsClave.map(bit => bit.bitEnviado).join("")}
CLAVE DE BOB (BITS): ${bitsClave.map(bit => bit.bitRecibido).join("")}
`;
    
    const blob = new Blob([contenido], { type: 'text/plain' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    
    a.href = url;
    a.download = `BB84_keys_${timestamp}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    window.URL.revokeObjectURL(url);
}

// Inicializar contenedores al cargar la página
document.addEventListener('DOMContentLoaded', function() {
    updateDurationRanges();
    
    // Inicializar visualizaciones vacías
    visualizarBits("bit-visualization-1", []);
    visualizarBits("validation-bits", []);
    visualizarBits("key-bits", []);
    visualizarBits("alice-key-bits", []);
    visualizarBits("bob-key-bits", []);
});

/**
 * Función auxiliar para convertir array de bits a representación hexadecimal
 */
function bitsAHexadecimal(bits) {
    let hex = "";
    // Procesar de 4 en 4 bits
    for (let i = 0; i < bits.length; i += 4) {
        let chunk = bits.slice(i, i + 4);
        // Rellenar con ceros si el último grupo no tiene 4 bits
        while (chunk.length < 4) {
            chunk.push("0");
        }
        // Convertir grupo de 4 bits a valor decimal
        let decimal = 0;
        for (let j = 0; j < chunk.length; j++) {
            decimal += parseInt(chunk[j]) * Math.pow(2, 3 - j);
        }
        // Convertir decimal a caracter hexadecimal
        hex += decimal.toString(16).toUpperCase();
    }
    return hex;
}

/**
 * Función para visualizar bits en la interfaz como bloques coloreados
 */
function visualizarBits(containerId, bits) {
    const container = document.getElementById(containerId);
    container.innerHTML = "";
    
    // Si no hay bits, mostrar mensaje
    if (!bits || bits.length === 0) {
        container.innerHTML = "<p class='no-data'>No hay datos disponibles</p>";
        return;
    }
    
    // Crear representación visual de bits
    bits.forEach(bit => {
        const bitElement = document.createElement("span");
        bitElement.className = "bit-box " + (bit === "1" ? "bit-one" : "bit-zero");
        bitElement.textContent = bit;
        container.appendChild(bitElement);
    });
}

/**
 * Función para demostrar la encriptación y desencriptación usando las claves generadas
 */
function demostrarEncriptacion() {
    // Verificar si tenemos claves generadas
    if (!claveAliceHex || !claveBobHex) {
        alert("⚠️ Debes generar las claves primero.");
        return;
    }
    
    // Obtener el texto a encriptar
    const textoPlano = document.getElementById("plain-text").value.trim();
    if (!textoPlano) {
        alert("⚠️ Introduce un texto para encriptar.");
        return;
    }
    
    // Obtener los bits de las claves
    const bitsAlice = bitsClave.map(bit => parseInt(bit.bitEnviado));
    const bitsBob = bitsClave.map(bit => parseInt(bit.bitRecibido));
    
    // Verificar si hay suficientes bits para encriptar el texto
    const maxCharsValue = Math.floor(bitsAlice.length / 8);
    if (textoPlano.length > maxCharsValue) {
        alert(`⚠️ El texto es demasiado largo. Solo puedes encriptar hasta ${maxCharsValue} caracteres con esta clave.`);
        return;
    }
    
    // Convertir el texto a array de bytes (valores ASCII)
    const bytesTexto = [];
    for (let i = 0; i < textoPlano.length; i++) {
        bytesTexto.push(textoPlano.charCodeAt(i));
    }
    
    // Encriptar usando la clave de Alice (XOR byte a byte)
    const bytesEncriptados = [];
    for (let i = 0; i < bytesTexto.length; i++) {
        let keyByte = 0;
        // Usar 8 bits de la clave para formar un byte
        for (let j = 0; j < 8; j++) {
            const keyIndex = (i * 8 + j) % bitsAlice.length;
            keyByte |= (bitsAlice[keyIndex] << (7 - j));
        }
        bytesEncriptados.push(bytesTexto[i] ^ keyByte);
    }
    
    // Convertir bytes encriptados a caracteres (cuando sea posible)
    let textoEncriptado = '';
    for (let i = 0; i < bytesEncriptados.length; i++) {
        // Agregar el carácter si es imprimible, o una representación si no lo es
        if (bytesEncriptados[i] >= 32 && bytesEncriptados[i] <= 126) {
            textoEncriptado += String.fromCharCode(bytesEncriptados[i]);
        } else {
            textoEncriptado += `\\x${bytesEncriptados[i].toString(16).padStart(2, '0')}`;
        }
    }
    
    // Desencriptar usando la clave de Bob
    const bytesDesencriptados = [];
    for (let i = 0; i < bytesEncriptados.length; i++) {
        let keyByte = 0;
        // Usar 8 bits de la clave para formar un byte
        for (let j = 0; j < 8; j++) {
            const keyIndex = (i * 8 + j) % bitsBob.length;
            keyByte |= (bitsBob[keyIndex] << (7 - j));
        }
        bytesDesencriptados.push(bytesEncriptados[i] ^ keyByte);
    }
    
    // Convertir bytes desencriptados de vuelta a texto
    let textoDesencriptado = '';
    for (let i = 0; i < bytesDesencriptados.length; i++) {
        textoDesencriptado += String.fromCharCode(bytesDesencriptados[i]);
    }
    
    // Mostrar los resultados
    document.getElementById("original-message").textContent = textoPlano;
    document.getElementById("encrypted-message").textContent = textoEncriptado;
    document.getElementById("decrypted-message").textContent = textoDesencriptado;
    
    // Verificar si hay diferencias entre las claves y mostrar advertencia
    const errorContainer = document.getElementById("decryption-error-container");
    const errorMessage = document.getElementById("decryption-error");
    
    if (bitsAlice.some((bit, index) => bit !== bitsBob[index] && index < bitsAlice.length)) {
        errorContainer.style.display = "block";
        errorMessage.textContent = "La desencriptación no es perfecta porque las claves de Alice y Bob tienen diferencias. Esto debido a errores en la transmisión cuántica que afectan al sistema.";
    } else {
        errorContainer.style.display = "none";
    }
}

/**
 * Función para convertir texto a representación binaria
 */
function textoABinario(texto) {
    let binario = [];
    for (let i = 0; i < texto.length; i++) {
        const codeByte = texto.charCodeAt(i);
        // Convertir cada caracter a 8 bits
        const byte = codeByte.toString(2).padStart(8, '0');
        for (let j = 0; j < byte.length; j++) {
            binario.push(byte[j]);
        }
    }
    return binario;
}

/**
 * Función para convertir representación binaria a texto
 */
function binarioATexto(binario) {
    let texto = "";
    // Procesar de 8 en 8 bits
    for (let i = 0; i < binario.length; i += 8) {
        let byte = binario.slice(i, i + 8);
        // Si no tenemos 8 bits completos, rellenar con ceros
        while (byte.length < 8) byte.push("0");
        
        // Convertir a valor decimal
        let decimal = 0;
        for (let j = 0; j < byte.length; j++) {
            decimal += parseInt(byte[j]) * Math.pow(2, 7 - j);
        }
        
        // Convertir a caracter
        texto += String.fromCharCode(decimal);
    }
    return texto;
}

/**
 * Función para encriptar o desencriptar usando operación XOR
 */
function encriptarXOR(mensaje, clave) {
    let resultado = [];
    // Usar la clave en forma cíclica si el mensaje es más largo
    for (let i = 0; i < mensaje.length; i++) {
        // Operación XOR: 1⊕1=0, 0⊕0=0, 1⊕0=1, 0⊕1=1
        const bit = (parseInt(mensaje[i]) ^ parseInt(clave[i % clave.length])).toString();
        resultado.push(bit);
    }
    return resultado;
}

/**
 * Función para convertir una cadena binaria a hexadecimal
 */
function binarioAHexadecimal(binario) {
    let hex = "";
    // Procesar de 4 en 4 bits
    for (let i = 0; i < binario.length; i += 4) {
        let chunk = binario.slice(i, i + 4);
        // Si no tenemos 4 bits completos, rellenar con ceros
        while (chunk.length < 4) chunk.push("0");
        
        // Convertir a valor decimal
        let decimal = 0;
        for (let j = 0; j < chunk.length; j++) {
            decimal += parseInt(chunk[j]) * Math.pow(2, 3 - j);
        }
        
        // Convertir a carácter hexadecimal
        hex += decimal.toString(16).toUpperCase();
    }
    return hex;
}

/**
 * Función para copiar texto al portapapeles
 */
function copiarTextoAlPortapapeles(elementId) {
    const texto = document.getElementById(elementId).textContent;
    if (!texto || texto === '-') {
        alert("⚠️ No hay texto para copiar.");
        return;
    }
    
    navigator.clipboard.writeText(texto).then(() => {
        alert("✅ Texto copiado al portapapeles.");
    }).catch(err => {
        console.error('Error al copiar: ', err);
        alert("❌ Error al copiar el texto. Intente nuevamente.");
    });
}

// Inicializar contenedores al cargar la página
document.addEventListener('DOMContentLoaded', function() {
    updateDurationRanges();
    
    // Inicializar visualizaciones vacías
    visualizarBits("bit-visualization-1", []);
    visualizarBits("validation-bits", []);
    visualizarBits("key-bits", []);
    visualizarBits("alice-key-bits", []);
    visualizarBits("bob-key-bits", []);
    
    // Inicializar el contador de caracteres
    setTimeout(actualizarContadorOriginal, 100);
    
    // Agregar listener para actualizar el contador cuando se cambie de pestaña
    const claveTab = document.querySelector('.nav-tab[onclick*="clave-tab"]');
    if (claveTab) {
        claveTab.addEventListener('click', function() {
            setTimeout(actualizarContadorOriginal, 200);
        });
    }
});

/**
 * Función para actualizar el contador de caracteres en la demostración de encriptación
 * Muestra cuántos caracteres se pueden encriptar basado en la longitud de la clave (8 bits por carácter)
 */
function actualizarContadorOriginal() {
    const textarea = document.getElementById("plain-text");
    const charCount = document.getElementById("char-count");
    const maxChars = document.getElementById("max-chars");
    
    if (!textarea || !charCount || !maxChars) {
        console.log("No se encontraron elementos del contador de caracteres");
        return; // Evitar errores si los elementos no existen
    }
    
    // Calcular el máximo de caracteres que se pueden encriptar (clave/8)
    let maxCharsValue = 0;
    
    if (bitsClave && bitsClave.length > 0) {
        maxCharsValue = Math.floor(bitsClave.length / 8);
    }
    
    // Actualizar los contadores
    charCount.textContent = textarea.value.length || 0;
    maxChars.textContent = maxCharsValue;
    
    // El contenedor del textarea para aplicar estilos
    const container = textarea.closest('.textarea-with-counter');
    
    // Cambiar color si se excede el límite
    if ((textarea.value.length || 0) > maxCharsValue) {
        charCount.style.color = "#f44336"; // Rojo
        if (container) {
            container.classList.add('character-limit-exceeded');
        }
    } else {
        charCount.style.color = ""; // Color por defecto
        if (container) {
            container.classList.remove('character-limit-exceeded');
        }
    }
}

// Actualizar el evento de generación de clave
document.getElementById('generate-key-btn').addEventListener('click', function() {
    // La función generarClaveBB84 ya se está llamando desde el HTML
    // pero aseguramos una actualización posterior del contador
    setTimeout(actualizarContadorOriginal, 500);
});

// Modificar el evento DOMContentLoaded
document.addEventListener('DOMContentLoaded', function() {
    updateDurationRanges();
    
    // Inicializar visualizaciones vacías
    visualizarBits("bit-visualization-1", []);
    visualizarBits("validation-bits", []);
    visualizarBits("key-bits", []);
    visualizarBits("alice-key-bits", []);
    visualizarBits("bob-key-bits", []);
    
    // Inicializar contadores de caracteres
    actualizarContadorOriginal();
    
    // Inicializar otros contadores si existen en esta página
    if (typeof actualizarContadorCorregido === 'function') {
        actualizarContadorCorregido();
    }
    
    if (typeof actualizarContadorAmplificado === 'function') {
        actualizarContadorAmplificado();
    }
    
    // Añadir listeners para cambio de pestañas
    const tabs = document.querySelectorAll('.nav-tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', function() {
            // Determinar qué pestaña se activó y actualizar el contador correspondiente
            if (this.getAttribute('onclick').includes('clave-tab')) {
                setTimeout(actualizarContadorOriginal, 300);
            } else if (this.getAttribute('onclick').includes('cascade-tab') && 
                      typeof actualizarContadorCorregido === 'function') {
                setTimeout(actualizarContadorCorregido, 300);
            } else if (this.getAttribute('onclick').includes('amplification-tab') && 
                      typeof actualizarContadorAmplificado === 'function') {
                setTimeout(actualizarContadorAmplificado, 300);
            }
        });
    });
    
    // También asignar evento a la casilla de texto para actualizar contador mientras se escribe
    const plainTextArea = document.getElementById('plain-text');
    if (plainTextArea) {
        plainTextArea.addEventListener('input', actualizarContadorOriginal);
    }
});

// ...existing code...

