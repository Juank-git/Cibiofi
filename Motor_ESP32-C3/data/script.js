// ==============================================
// MOTOR CARACTERIZADOR - HTTP REST Client
// ==============================================

let isConnected = false;
let motorData = {
    status: 'idle',
    homed: false,
    angle: 0.0,
    targetAngle: 0.0,
    minAngle: 0.15,
    moving: false
};

// Estado del barrido
let sweepState = {
    active: false,
    currentRep: 0,
    totalReps: 0,
    currentStep: 0,
    totalSteps: 0,
    startAngle: 0,
    endAngle: 0,
    stepAngle: 0,
    delay: 500,
    angles: []
};

// Elementos DOM
const wsIndicator = document.getElementById('wsIndicator');
const wsText = document.getElementById('wsText');
const motorStatus = document.getElementById('motorStatus');
const homedStatus = document.getElementById('homedStatus');
const currentAngle = document.getElementById('currentAngle');
const targetAngle = document.getElementById('targetAngle');
const minAngleDisplay = document.getElementById('minAngle');
const angleInput = document.getElementById('angleInput');
const angleError = document.getElementById('angleError');
const homeBtn = document.getElementById('homeBtn');
const moveBtn = document.getElementById('moveBtn');
const stopBtn = document.getElementById('stopBtn');
const presetButtons = document.querySelectorAll('.btn-preset');

// Elementos de barrido
const sweepStart = document.getElementById('sweepStart');
const sweepEnd = document.getElementById('sweepEnd');
const sweepStep = document.getElementById('sweepStep');
const sweepReps = document.getElementById('sweepReps');
const sweepDelay = document.getElementById('sweepDelay');
const sweepStepsCount = document.getElementById('sweepStepsCount');
const sweepBtn = document.getElementById('sweepBtn');
const sweepStopBtn = document.getElementById('sweepStopBtn');
const sweepProgress = document.getElementById('sweepProgress');
const sweepError = document.getElementById('sweepError');
const currentRep = document.getElementById('currentRep');
const totalReps = document.getElementById('totalReps');
const currentStep = document.getElementById('currentStep');
const totalSteps = document.getElementById('totalSteps');
const sweepCurrentAngle = document.getElementById('sweepCurrentAngle');
const progressFill = document.getElementById('progressFill');

// ==============================================
// HTTP API
// ==============================================

const API_BASE = '';  // Usar mismo host

async function fetchStatus() {
    try {
        const response = await fetch(`${API_BASE}/api/status`);
        if (!response.ok) throw new Error('HTTP ' + response.status);
        
        const data = await response.json();
        
        if (!isConnected) {
            isConnected = true;
            updateConnectionStatus(true);
        }
        
        updateMotorData(data);
    } catch (error) {
        console.error('Error obteniendo estado:', error);
        if (isConnected) {
            isConnected = false;
            updateConnectionStatus(false);
        }
    }
}

async function sendCommand(endpoint, data = null) {
    try {
        const options = {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        };
        
        if (data) {
            options.body = JSON.stringify(data);
        }
        
        const response = await fetch(`${API_BASE}${endpoint}`, options);
        if (!response.ok) throw new Error('HTTP ' + response.status);
        
        const result = await response.json();
        console.log('Comando enviado:', endpoint, result);
        
        // Actualizar estado inmediatamente después del comando
        await fetchStatus();
        
        return result;
    } catch (error) {
        console.error('Error enviando comando:', error);
        alert('Error: No se pudo enviar el comando');
        throw error;
    }
}

function updateConnectionStatus(connected) {
    if (connected) {
        wsIndicator.className = 'indicator connected';
        wsText.textContent = 'Conectado';
    } else {
        wsIndicator.className = 'indicator disconnected';
        wsText.textContent = 'Desconectado';
    }
}

// ==============================================
// ACTUALIZACIÓN DE DATOS
// ==============================================

function updateMotorData(data) {
    // Actualizar objeto local
    motorData.status = data.status || motorData.status;
    motorData.homed = data.homed !== undefined ? data.homed : motorData.homed;
    motorData.angle = data.angle !== undefined ? data.angle : motorData.angle;
    motorData.targetAngle = data.targetAngle !== undefined ? data.targetAngle : motorData.targetAngle;
    motorData.minAngle = data.minAngle !== undefined ? data.minAngle : motorData.minAngle;
    motorData.moving = data.moving !== undefined ? data.moving : motorData.moving;
    
    // Actualizar UI
    updateUI();
}

function updateUI() {
    // Estado del motor
    let statusText = motorData.status.toUpperCase();
    let statusClass = motorData.status;
    motorStatus.textContent = statusText;
    motorStatus.className = `value status-${statusClass}`;
    
    // Estado de homing
    homedStatus.textContent = motorData.homed ? 'SÍ' : 'NO';
    homedStatus.className = motorData.homed ? 'value homed-yes' : 'value homed-no';
    
    // Ángulo actual
    currentAngle.textContent = motorData.angle.toFixed(2) + '°';
    
    // Ángulo objetivo
    if (motorData.targetAngle > 0) {
        targetAngle.textContent = motorData.targetAngle.toFixed(2) + '°';
    } else {
        targetAngle.textContent = '---';
    }
    
    // Ángulo mínimo
    minAngleDisplay.textContent = motorData.minAngle.toFixed(2) + '° (' +
        (360 / motorData.minAngle).toFixed(0) + ' pasos/rev)';
    
    // Habilitar/deshabilitar botones
    const isBusy = motorData.status === 'moving' || motorData.status === 'homing';
    moveBtn.disabled = !motorData.homed || isBusy || !isConnected || sweepState.active;
    homeBtn.disabled = isBusy || !isConnected || sweepState.active;
    angleInput.disabled = isBusy || !isConnected || sweepState.active;
    sweepBtn.disabled = !motorData.homed || isBusy || !isConnected || sweepState.active;
    sweepStopBtn.disabled = !sweepState.active;
    
    // Presets
    presetButtons.forEach(btn => {
        btn.disabled = !motorData.homed || isBusy || !isConnected || sweepState.active;
    });
}

// ==============================================
// VALIDACIÓN DE ÁNGULOS
// ==============================================

function validateAngle(angle) {
    const errors = [];
    
    // Rango válido
    if (angle < 0 || angle > 360) {
        errors.push('El ángulo debe estar entre 0° y 360°');
    }
    
    // Múltiplo del ángulo mínimo
    const steps = angle / motorData.minAngle;
    const roundedSteps = Math.round(steps);
    const diff = Math.abs(steps - roundedSteps);
    
    if (diff > 0.001) {
        const correctedAngle = (roundedSteps * motorData.minAngle).toFixed(2);
        errors.push(`El ángulo debe ser múltiplo de ${motorData.minAngle.toFixed(2)}°. Redondeado: ${correctedAngle}°`);
    }
    
    return errors;
}

function roundToMinAngle(angle) {
    const steps = Math.round(angle / motorData.minAngle);
    return steps * motorData.minAngle;
}

// ==============================================
// EVENT HANDLERS
// ==============================================

homeBtn.addEventListener('click', async () => {
    if (confirm('¿Iniciar proceso de homing?')) {
        console.log('Iniciating homing...');
        await sendCommand('/api/home');
    }
});

moveBtn.addEventListener('click', async () => {
    const angle = parseFloat(angleInput.value);
    
    if (isNaN(angle)) {
        angleError.textContent = 'Ingrese un ángulo válido';
        return;
    }
    
    // Validar
    const errors = validateAngle(angle);
    if (errors.length > 0) {
        angleError.textContent = errors[0];
        
        // Ofrecer corrección automática
        const corrected = roundToMinAngle(angle);
        if (confirm(`${errors[0]}\n\n¿Usar ${corrected.toFixed(2)}° en su lugar?`)) {
            angleInput.value = corrected.toFixed(2);
            angleError.textContent = '';
            await sendCommand('/api/move', { angle: corrected });
        }
        return;
    }
    
    angleError.textContent = '';
    console.log(`Moviendo a ${angle}°`);
    await sendCommand('/api/move', { angle: angle });
});

stopBtn.addEventListener('click', async () => {
    console.log('STOP - Deteniendo motor');
    await sendCommand('/api/stop');
});

// Validación en tiempo real del input
angleInput.addEventListener('input', () => {
    const angle = parseFloat(angleInput.value);
    if (!isNaN(angle)) {
        const errors = validateAngle(angle);
        angleError.textContent = errors.length > 0 ? errors[0] : '';
    } else {
        angleError.textContent = '';
    }
});

// Ángulos predefinidos
presetButtons.forEach(btn => {
    btn.addEventListener('click', async () => {
        const angle = parseFloat(btn.getAttribute('data-angle'));
        angleInput.value = angle.toFixed(2);
        angleError.textContent = '';
        
        // Enviar automáticamente si está homed
        if (motorData.homed && !motorData.moving) {
            await sendCommand('/api/move', { angle: angle });
        }
    });
});

// Enter en el input
angleInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter' && !moveBtn.disabled) {
   

// ==============================================
// BARRIDO ANGULAR
// ==============================================

function calculateSweepSteps() {
    const start = parseFloat(sweepStart.value) || 0;
    const end = parseFloat(sweepEnd.value) || 0;
    const step = parseFloat(sweepStep.value) || 1;
    calculateSweepSteps();
    
    if (step <= 0) {
        sweepStepsCount.textContent = 'Error: paso debe ser > 0';
        return 0;
    }
    
    const steps = Math.floor(Math.abs(end - start) / step) + 1;
    sweepStepsCount.textContent = steps;
    return steps;
}

function generateSweepAngles() {
    const start = roundToMinAngle(parseFloat(sweepStart.value) || 0);
    const end = roundToMinAngle(parseFloat(sweepEnd.value) || 0);
    const step = roundToMinAngle(parseFloat(sweepStep.value) || 1);
    
    const angles = [];
    if (start <= end) {
        for (let angle = start; angle <= end; angle += step) {
            angles.push(roundToMinAngle(angle));
        }
    } else {
        for (let angle = start; angle >= end; angle -= step) {
            angles.push(roundToMinAngle(angle));
        }
    }
    
    return angles;
}

function updateSweepProgress() {
    const totalStepsCalc = sweepState.angles.length * sweepState.totalReps;
    const completedSteps = (sweepState.currentRep - 1) * sweepState.angles.length + sweepState.currentStep;
    const progress = (completedSteps / totalStepsCalc) * 100;
    
    currentRep.textContent = sweepState.currentRep;
    totalReps.textContent = sweepState.totalReps;
    currentStep.textContent = sweepState.currentStep;
    totalSteps.textContent = sweepState.angles.length;
    
    const currentAngle = sweepState.angles[sweepState.currentStep - 1] || 0;
    sweepCurrentAngle.textContent = currentAngle.toFixed(2) + '°';
    
    progressFill.style.width = progress.toFixed(1) + '%';
}

async function executeSweep() {
    if (!motorData.homed) {
        sweepError.textContent = 'Error: Realizar homing primero';
        return;
    }
    
    // Generar lista de ángulos
    sweepState.angles = generateSweepAngles();
    sweepState.totalReps = parseInt(sweepReps.value) || 1;
    sweepState.delay = parseInt(sweepDelay.value) || 500;
    
    if (sweepState.angles.length === 0) {
        sweepError.textContent = 'Error: No se generaron ángulos válidos';
        return;
    }
    
    sweepState.active = true;
    sweepState.currentRep = 0;
    sweepState.currentStep = 0;
    sweepProgress.style.display = 'block';
    sweepError.textContent = '';
    updateUI();
    
    console.log('Iniciando barrido:', sweepState);
    
    // Ejecutar repeticiones
    for (let rep = 1; rep <= sweepState.totalReps && sweepState.active; rep++) {
        sweepState.currentRep = rep;
        
        // Ejecutar pasos
        for (let i = 0; i < sweepState.angles.length && sweepState.active; i++) {
            sweepState.currentStep = i + 1;
            const angle = sweepState.angles[i];
            
            updateSweepProgress();
            
            // Enviar comando de movimiento
            await sendCommand('/api/move', { angle: angle });
            
            // Esperar a que el motor termine de moverse
            await waitForMotorIdle();
            
            // Delay entre pasos
            if (sweepState.delay > 0) {
                await sleep(sweepState.delay);
            }
        }
    }
    
    // Volver a 0° al finalizar
    if (sweepState.active) {
        console.log('Barrido completado, retornando a 0°');
        await sendCommand('/api/move', { angle: 0 });
        await waitForMotorIdle();
    }
    
    stopSweep();
}

function stopSweep() {
    sweepState.active = false;
    sweepProgress.style.display = 'none';
    console.log('Barrido detenido');
    updateUI();
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function waitForMotorIdle() {
    while (motorData.status !== 'idle' || motorData.moving) {
        await new Promise(resolve => setTimeout(resolve, 200));
        await fetchStatus();
    }
}

// Event listeners para barrido
sweepStart.addEventListener('input', calculateSweepSteps);
sweepEnd.addEventListener('input', calculateSweepSteps);
sweepStep.addEventListener('input', calculateSweepSteps);

sweepBtn.addEventListener('click', () => {
    if (confirm(`¿Iniciar barrido de ${sweepStart.value}° a ${sweepEnd.value}° con paso de ${sweepStep.value}° y ${sweepReps.value} repeticiones?`)) {
        executeSweep();
    }
});

sweepStopBtn.addEventListener('click', async () => {
    if (confirm('¿Detener el barrido angular?')) {
        stopSweep();
        await sendCommand('/api/stop');
    }
});     moveBtn.click();
    }
});

// ==============================================
// INICIALIZACIÓN
// ==============================================

let statusPollingInterval = null;

function startStatusPolling() {
    // Poll cada 500ms
    statusPollingInterval = setInterval(fetchStatus, 500);
    // Fetch inicial inmediato
    fetchStatus();
}

function stopStatusPolling() {
    if (statusPollingInterval) {
        clearInterval(statusPollingInterval);
        statusPollingInterval = null;
    }
}

window.addEventListener('load', () => {
    console.log('Interfaz cargada - iniciando polling HTTP');
    startStatusPolling();
});

window.addEventListener('beforeunload', () => {
    stopStatusPolling();
});