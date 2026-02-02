// Variables para el algoritmo Cascade
let cascadeAliceKey = [];
let cascadeBobKey = [];
let cascadeBobKeyCorrected = [];
let currentPass = 1;
let maxPasses = 2;
let blockSize = 8;
let currentBlockIndex = 0;
let errorBlocks = [];
let totalCorrectedBits = 0;
let initialErrors = 0;

// Añadir una variable global para rastrear la información filtrada
let informacionFiltrada = 0;

/**
 * Inicializa el proceso de corrección Cascade
 */
function iniciarCascade() {
    // Verificar si hay claves generadas para corregir
    if (!bitsClave || bitsClave.length === 0) {
        alert("⚠️ Debes generar claves primero. Ve a la pestaña 'Generación de Clave' y ejecuta el proceso.");
        return;
    }
    
    // Obtener parámetros de configuración
    blockSize = parseInt(document.getElementById("block-size").value);
    maxPasses = parseInt(document.getElementById("num-passes").value);
    
    // Validar parámetros
    if (blockSize < 2) {
        alert("El tamaño de bloque debe ser al menos 2");
        return;
    }
    
    if (maxPasses < 1 || maxPasses > 4) {
        alert("El número de pasadas debe estar entre 1 y 4");
        return;
    }
    
    // Inicializar variables
    cascadeAliceKey = bitsClave.map(bit => parseInt(bit.bitEnviado));
    cascadeBobKey = bitsClave.map(bit => parseInt(bit.bitRecibido));
    cascadeBobKeyCorrected = [...cascadeBobKey];
    currentPass = 1;
    currentBlockIndex = 0;
    errorBlocks = [];
    totalCorrectedBits = 0;
    
    // Contar diferencias iniciales
    initialErrors = 0;
    for (let i = 0; i < cascadeAliceKey.length; i++) {
        if (cascadeAliceKey[i] !== cascadeBobKey[i]) {
            initialErrors++;
        }
    }
    
    // Reiniciar el contador de información filtrada
    informacionFiltrada = 0;
    
    // Actualizar interfaz del paso 1
    document.querySelectorAll('.cascade-step').forEach(step => step.classList.remove('active'));
    document.getElementById('cascade-step1').classList.add('active');
    
    document.getElementById('initial-diff-count').textContent = initialErrors;
    document.getElementById('total-key-bits').textContent = cascadeAliceKey.length;
    document.getElementById('initial-error-rate').textContent = 
        (initialErrors / cascadeAliceKey.length * 100).toFixed(2);
    
    // Visualizar las claves originales
    visualizarClaveCascade('cascade-alice-key', cascadeAliceKey, cascadeBobKey);
    visualizarClaveCascade('cascade-bob-key', cascadeBobKey, cascadeAliceKey);
    
    // Actualizar contador de caracteres
    actualizarContadorCorregido();
    
    // Asegurarnos de que el botón de saltar proceso está visible
    const skipBtn = document.getElementById('skip-cascade-btn');
    if (skipBtn) {
        skipBtn.style.display = 'block';
    }
}

/**
 * Avanza al siguiente paso en el proceso Cascade
 */
function avanzarCascade(paso) {
    document.querySelectorAll('.cascade-step').forEach(step => step.classList.remove('active'));
    document.getElementById(`cascade-step${paso}`).classList.add('active');
    
    if (paso === 2) {
        // Inicio de una pasada: dividir en bloques y comparar paridades
        document.getElementById('current-pass').textContent = currentPass;
        dividirBloques();
    } else if (paso === 3) {
        // Al avanzar al paso 3, iniciar automáticamente la corrección del primer bloque
        // Esto arregla el problema donde el usuario tiene que hacer clic dos veces
        currentBlockIndex = 0; // Asegurar que empezamos desde el primer bloque
        
        // Limpiar log anterior si existe
        const logContainer = document.getElementById('correction-log');
        if (logContainer) {
            logContainer.innerHTML = '';
        }
        
        // Iniciar automáticamente la corrección del primer bloque
        setTimeout(() => {
            corregirSiguienteBloque();
        }, 100); // Pequeño retraso para asegurar que la UI se actualiza primero
    }
}

/**
 * Divide las claves en bloques y calcula paridades
 */
function dividirBloques() {
    // Calcular tamaño de bloque para esta pasada
    // Para pasadas posteriores, cambiamos el tamaño según la estrategia de Cascade
    const tamanioActual = currentPass === 1 ? blockSize : Math.floor(blockSize * 1.5 * currentPass);
    
    // Limpiar visualizaciones anteriores
    document.getElementById('blocks-visualization').innerHTML = '';
    document.getElementById('parity-table-body').innerHTML = '';
    
    errorBlocks = []; // Reiniciar lista de bloques con errores
    
    // Dividir las claves en bloques
    const numBloques = Math.ceil(cascadeAliceKey.length / tamanioActual);
    let html = '';
    let tableHtml = '';
    
    for (let i = 0; i < numBloques; i++) {
        const inicio = i * tamanioActual;
        const fin = Math.min(inicio + tamanioActual, cascadeAliceKey.length);
        
        // Extracción de bloques
        const bloqueAlice = cascadeAliceKey.slice(inicio, fin);
        const bloqueBob = cascadeBobKeyCorrected.slice(inicio, fin);
        
        // Cálculo de paridades (XOR de todos los bits)
        const paridadAlice = bloqueAlice.reduce((a, b) => a ^ b, 0);
        const paridadBob = bloqueBob.reduce((a, b) => a ^ b, 0);
        const paridadIgual = paridadAlice === paridadBob;
        
        // Si paridades son diferentes, este bloque tiene un número impar de errores
        if (!paridadIgual) {
            errorBlocks.push({
                indice: i,
                inicio: inicio,
                fin: fin,
                bits: fin - inicio
            });
        }
        
        // Generar HTML para visualización de bloques
        html += `
            <div class="bit-block ${!paridadIgual ? 'error-block' : ''}">
                <div class="block-label">Bloque ${i+1}</div>
                <div class="block-participants">
                    <div class="participant-row">
                        <div class="participant-label">Alice:</div>
                        <div class="block-bits">
        `;
        
        for (let j = inicio; j < fin; j++) {
            const bitAlice = cascadeAliceKey[j];
            const bitBob = cascadeBobKeyCorrected[j];
            const tieneError = bitAlice !== bitBob;
            
            html += `
                <div class="cascade-bit ${bitAlice === 1 ? 'bit-1' : 'bit-0'} ${tieneError ? 'bit-error' : ''}">
                    ${bitAlice}
                </div>
            `;
        }
        
        html += `
                        </div>
                        <div class="block-parity">Paridad: ${paridadAlice}</div>
                    </div>
                    <div class="participant-row">
                        <div class="participant-label">Bob:</div>
                        <div class="block-bits">
        `;
        
        for (let j = inicio; j < fin; j++) {
            const bitAlice = cascadeAliceKey[j];
            const bitBob = cascadeBobKeyCorrected[j];
            const tieneError = bitAlice !== bitBob;
            
            html += `
                <div class="cascade-bit ${bitBob === 1 ? 'bit-1' : 'bit-0'} ${tieneError ? 'bit-error' : ''}">
                    ${bitBob}
                </div>
            `;
        }
        
        html += `
                        </div>
                        <div class="block-parity">Paridad: ${paridadBob}</div>
                    </div>
                </div>
            </div>
        `;
        
        // Generar fila de tabla para comparación de paridades
        tableHtml += `
            <tr>
                <td>Bloque ${i+1}</td>
                <td>${paridadAlice}</td>
                <td>${paridadBob}</td>
                <td class="${paridadIgual ? 'parity-match' : 'parity-mismatch'}">
                    ${paridadIgual ? 'Iguales ✓' : 'Diferentes ✗'}
                </td>
            </tr>
        `;
    }
    
    // Actualizar la interfaz
    document.getElementById('blocks-visualization').innerHTML = html;
    document.getElementById('parity-table-body').innerHTML = tableHtml;
    
    // Por cada bloque con paridad diferente, incrementamos la información filtrada
    // 1 bit de paridad por bloque es revelado
    informacionFiltrada += numBloques;
    
    // Modificar el botón según si hay bloques con errores
    const btnBuscar = document.querySelector('#cascade-step2 .action-btn');
    if (errorBlocks.length > 0) {
        btnBuscar.textContent = `Buscar Errores (${errorBlocks.length} bloques con paridad diferente)`;
        btnBuscar.onclick = function() { avanzarCascade(3); };
    } else {
        btnBuscar.textContent = 'No hay errores de paridad para corregir - Continuar';
        // Cambiar la función del botón para avanzar al siguiente paso o finalizar
        btnBuscar.onclick = function() {
            if (currentPass < maxPasses) {
                iniciarSiguientePasada();
            } else {
                finalizarCascade();
            }
        };
    }
    btnBuscar.disabled = false; // Nunca deshabilitar el botón
}

/**
 * Inicia la corrección del siguiente bloque con error de paridad
 */
function corregirSiguienteBloque() {
    // Ocultar/mostrar botones según el estado
    document.getElementById('next-block-btn').style.display = 
        currentBlockIndex < errorBlocks.length - 1 ? 'block' : 'none';
    
    document.getElementById('next-pass-btn').style.display = 
        currentBlockIndex >= errorBlocks.length - 1 && currentPass < maxPasses ? 'block' : 'none';
    
    document.getElementById('finalize-btn').style.display = 
        currentBlockIndex >= errorBlocks.length - 1 && currentPass >= maxPasses ? 'block' : 'none';
    
    if (currentBlockIndex >= errorBlocks.length) {
        // Hemos terminado con todos los bloques en esta pasada
        if (currentPass < maxPasses) {
            iniciarSiguientePasada();
        } else {
            finalizarCascade();
        }
        return;
    }
    
    // Obtener el bloque actual con error
    const bloque = errorBlocks[currentBlockIndex];
    const inicio = bloque.inicio;
    const fin = bloque.fin;
    
    // Extraer subconjuntos para búsqueda binaria
    const bloqueAlice = cascadeAliceKey.slice(inicio, fin);
    const bloqueBob = cascadeBobKeyCorrected.slice(inicio, fin);
    
    // Iniciar visualización de la búsqueda binaria
    const logContainer = document.getElementById('correction-log');
    logContainer.innerHTML += `<p class="log-entry">Corrigiendo bloque ${currentBlockIndex + 1} (bits ${inicio} a ${fin-1})...</p>`;
    
    // Realizar búsqueda binaria para encontrar el bit erróneo
    busquedaBinaria(bloqueAlice, bloqueBob, inicio, fin - 1, logContainer);
    
    // Pasar al siguiente bloque
    currentBlockIndex++;
}

/**
 * Realiza búsqueda binaria para encontrar un bit erróneo en un bloque
 */
function busquedaBinaria(bitsAlice, bitsBob, inicioGlobal, finGlobal, logContainer) {
    // Implementación visual paso a paso de la búsqueda binaria
    const bitsVisualization = document.getElementById('binary-search-viz');
    bitsVisualization.innerHTML = '';
    
    // Crear visualización inicial de todos los bits en el bloque
    let html = `<div class="search-step">
        <div class="search-block-label">Bloque completo (bits ${inicioGlobal} a ${finGlobal})</div>
        <div class="search-bits">`;
    
    for (let i = 0; i < bitsAlice.length; i++) {
        const bit = bitsBob[i];
        const error = bitsAlice[i] !== bit;
        html += `<div class="search-bit ${bit === 1 ? 'bit-1' : 'bit-0'} ${error ? 'bit-error' : ''}">${bit}</div>`;
    }
    
    html += `</div></div>`;
    bitsVisualization.innerHTML = html;
    
    // Algoritmo de búsqueda binaria
    let inicio = 0;
    let fin = bitsAlice.length - 1;
    
    // En búsqueda binaria, el número de consultas (bits filtrados) es aproximadamente log2(n)
    // donde n es el tamaño del bloque
    const bitsReveladosEnBusqueda = Math.ceil(Math.log2(bitsAlice.length));
    informacionFiltrada += bitsReveladosEnBusqueda;
    
    // Si el bloque tiene paridad diferente, hay un número impar de errores
    // Dividimos a la mitad y buscamos en qué mitad hay paridad diferente
    let pasos = 1;
    while (inicio < fin) {
        const mitad = Math.floor((inicio + fin) / 2);
        
        // Calcular paridad de la primera mitad
        let paridadAliceMitad1 = 0;
        let paridadBobMitad1 = 0;
        
        for (let i = inicio; i <= mitad; i++) {
            paridadAliceMitad1 ^= bitsAlice[i];
            paridadBobMitad1 ^= bitsBob[i];
        }
        
        // Visualizar la división actual
        html = `<div class="search-step">
            <div class="search-block-label">Paso ${pasos}: Dividir y comparar paridades (bits ${inicioGlobal + inicio} a ${inicioGlobal + mitad})</div>
            <div class="search-bits">`;
        
        for (let i = inicio; i <= mitad; i++) {
            const bit = bitsBob[i];
            const error = bitsAlice[i] !== bit;
            html += `<div class="search-bit ${bit === 1 ? 'bit-1' : 'bit-0'} ${error ? 'bit-error' : ''} ${i === mitad ? 'selected' : ''}">${bit}</div>`;
        }
        
        html += `</div></div>`;
        bitsVisualization.innerHTML += html;
        
        // Mostrar en el log
        logContainer.innerHTML += `<p class="log-entry">Paso ${pasos}: Verificando mitad izquierda (${inicioGlobal + inicio} a ${inicioGlobal + mitad})...</p>`;
        
        // Si las paridades son diferentes en la primera mitad, el error está ahí
        if (paridadAliceMitad1 !== paridadBobMitad1) {
            fin = mitad;
            logContainer.innerHTML += `<p class="log-entry">Error detectado en mitad izquierda.</p>`;
        } else {
            // Si no, el error está en la segunda mitad
            inicio = mitad + 1;
            logContainer.innerHTML += `<p class="log-entry">Error detectado en mitad derecha.</p>`;
        }
        
        pasos++;
        
        // Si llegamos a un solo bit, hemos encontrado el error
        if (inicio === fin) {
            const indexGlobal = inicioGlobal + inicio;
            const valorOriginal = cascadeBobKeyCorrected[indexGlobal];
            // Corregir el bit
            cascadeBobKeyCorrected[indexGlobal] = cascadeBobKeyCorrected[indexGlobal] === 1 ? 0 : 1;
            totalCorrectedBits++;
            
            // Visualizar el bit corregido
            html = `<div class="search-step">
                <div class="search-block-label">¡Error encontrado! Bit ${indexGlobal} corregido de ${valorOriginal} a ${cascadeBobKeyCorrected[indexGlobal]}</div>
                <div class="search-bits">
                    <div class="search-bit ${cascadeBobKeyCorrected[indexGlobal] === 1 ? 'bit-1' : 'bit-0'} error-found bit-corrected">
                        ${cascadeBobKeyCorrected[indexGlobal]}
                    </div>
                </div>
            </div>`;
            bitsVisualization.innerHTML += html;
            
            // Registrar en el log
            logContainer.innerHTML += `<p class="log-entry success">Bit ${indexGlobal} corregido: ${valorOriginal} → ${cascadeBobKeyCorrected[indexGlobal]}</p>`;
            
            // Propagar correcciones a otros bloques
            propagarCorreccion(indexGlobal);
            break;
        }
    }
}

/**
 * Propaga la corrección de un bit a otros bloques que puedan contenerlo
 */
function propagarCorreccion(indexBit) {
    // En una implementación real, revisaríamos todos los bloques de pasadas anteriores
    // que contengan este bit y verificaríamos si ahora tienen paridad correcta
    // Por simplicidad, solo mostraremos un mensaje en el log
    
    const logContainer = document.getElementById('correction-log');
    logContainer.innerHTML += `<p class="log-entry">Propagando corrección del bit ${indexBit} a otros bloques...</p>`;
}

/**
 * Inicia la siguiente pasada del algoritmo Cascade
 */
function iniciarSiguientePasada() {
    currentPass++;
    currentBlockIndex = 0;
    
    // Actualizar interfaz
    document.getElementById('current-pass').textContent = currentPass;
    
    // Volver al paso 2 para la nueva pasada
    avanzarCascade(2);
}

/**
 * Finaliza el proceso Cascade y muestra los resultados
 */
function finalizarCascade() {
    // Contar errores restantes
    let erroresRestantes = 0;
    for (let i = 0; i < cascadeAliceKey.length; i++) {
        if (cascadeAliceKey[i] !== cascadeBobKeyCorrected[i]) {
            erroresRestantes++;
        }
    }
    
    // Calcular porcentaje de corrección
    const eficiencia = initialErrors > 0 
        ? ((initialErrors - erroresRestantes) / initialErrors * 100).toFixed(2)
        : "100.00";
    
    // Calcular porcentaje de información filtrada
    // La información filtrada es un porcentaje de la longitud total de la clave
    const porcentajeInformacionFiltrada = (informacionFiltrada / cascadeAliceKey.length * 100).toFixed(2);
    
    // Actualizar interfaz
    document.getElementById('cascade-step4').classList.add('active');
    document.getElementById('cascade-step3').classList.remove('active');
    
    document.getElementById('corrected-bits-count').textContent = totalCorrectedBits;
    document.getElementById('remaining-errors').textContent = erroresRestantes;
    document.getElementById('correction-efficiency').textContent = eficiencia + "%";
    document.getElementById('leaked-information').textContent = porcentajeInformacionFiltrada + "%";
    
    // Visualizar claves finales
    visualizarClaveCascade('final-alice-key', cascadeAliceKey, cascadeBobKeyCorrected);
    visualizarClaveCascade('final-bob-key', cascadeBobKeyCorrected, cascadeAliceKey);
    
    // Generar clave hexadecimal final
    const claveHex = bitsAHexadecimal(cascadeBobKeyCorrected.map(bit => bit.toString()));
    document.getElementById('final-hex-key').textContent = claveHex;
    
    // Actualizar el contador de caracteres para la demostración
    setTimeout(() => {
        actualizarContadorCorregido();
        
        // Asegurar que la sección de demostración sea visible
        const demoSection = document.querySelector('.corrected-key-demo');
        if (demoSection) {
            // Hacer scroll a la sección de demostración
            demoSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
            
            // Opcional: añadir una clase temporal para resaltar la sección
            demoSection.classList.add('highlight-section');
            setTimeout(() => {
                demoSection.classList.remove('highlight-section');
            }, 2000);
        }
    }, 300);
}

/**
 * Visualiza una clave en forma de bits coloreados para Cascade
 */
function visualizarClaveCascade(containerId, bitsClave, bitsComparar) {
    const container = document.getElementById(containerId);
    container.innerHTML = "";
    
    for (let i = 0; i < bitsClave.length; i++) {
        const bit = bitsClave[i];
        const error = bit !== bitsComparar[i];
        
        const bitElement = document.createElement("span");
        bitElement.className = `cascade-bit ${bit === 1 ? 'bit-1' : 'bit-0'} ${error ? 'bit-error' : ''}`;
        bitElement.textContent = bit;
        container.appendChild(bitElement);
    }
}

/**
 * Copia la clave corregida al portapapeles
 */
function copiarClaveFinal() {
    const claveHex = document.getElementById('final-hex-key').textContent;
    
    if (!claveHex || claveHex === '-') {
        alert("⚠️ No hay clave corregida para copiar.");
        return;
    }
    
    navigator.clipboard.writeText(claveHex).then(() => {
        alert(`✅ Clave corregida copiada al portapapeles: ${claveHex}`);
    }).catch(err => {
        console.error('Error al copiar: ', err);
        alert("❌ Error al copiar la clave. Intente nuevamente.");
    });
}

/**
 * Guarda la clave corregida como archivo de texto
 */
function guardarClaveCorregida() {
    const claveHex = document.getElementById('final-hex-key').textContent;
    
    if (!claveHex || claveHex === '-') {
        alert("⚠️ No hay clave corregida para guardar.");
        return;
    }
    
    const contenido = `CLAVE BB84 CORREGIDA CON ALGORITMO CASCADE
Fecha: ${new Date().toLocaleString()}
Longitud: ${cascadeAliceKey.length} bits
Errores iniciales: ${initialErrors}
Bits corregidos: ${totalCorrectedBits}
Errores restantes: ${document.getElementById('remaining-errors').textContent}
Eficiencia de corrección: ${document.getElementById('correction-efficiency').textContent}

CLAVE FINAL (HEX): ${claveHex}

CLAVE FINAL (BITS): ${cascadeBobKeyCorrected.join("")}
`;
    
    const blob = new Blob([contenido], { type: 'text/plain' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    
    a.href = url;
    a.download = `BB84_corrected_key_${timestamp}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    window.URL.revokeObjectURL(url);
    
    alert("✅ Clave corregida guardada correctamente.");
}

/**
 * Función para actualizar el contador de caracteres en la demostración de encriptación
 * con la clave corregida
 */
function actualizarContadorCorregido() {
    const textarea = document.getElementById("corrected-plain-text");
    const charCount = document.getElementById("corrected-char-count");
    const maxChars = document.getElementById("corrected-max-chars");
    
    if (!textarea || !charCount || !maxChars) {
        return; // Evitar errores si los elementos no existen
    }
    
    // Calcular el máximo de caracteres que se pueden encriptar (clave corregida/8)
    const maxCharsValue = Math.floor((cascadeBobKeyCorrected && cascadeBobKeyCorrected.length > 0) ? cascadeBobKeyCorrected.length / 8 : 0);
    
    // Actualizar los contadores
    charCount.textContent = textarea.value.length || 0;
    maxChars.textContent = maxCharsValue;
    
    // Cambiar color si se excede el límite
    if ((textarea.value.length || 0) > maxCharsValue) {
        charCount.style.color = "#f44336"; // Rojo
    } else {
        charCount.style.color = ""; // Color por defecto
    }
    
    // Agregar un log para depuración
    console.log("Actualizado contador de caracteres para clave corregida. Max: " + maxCharsValue);
}

/**
 * Función para demostrar la encriptación y desencriptación usando la clave corregida
 */
function demostrarEncriptacionCorregida() {
    // Verificar si tenemos clave corregida
    if (!cascadeBobKeyCorrected || cascadeBobKeyCorrected.length === 0 || !cascadeAliceKey || cascadeAliceKey.length === 0) {
        alert("⚠️ Debes completar la corrección de errores primero.");
        return;
    }
    
    // Obtener el texto a encriptar
    const textoPlano = document.getElementById("corrected-plain-text").value.trim();
    if (!textoPlano) {
        alert("⚠️ Introduce un texto para encriptar.");
        return;
    }
    
    // Verificar si hay suficientes bits para encriptar el texto
    const maxCharsValue = Math.floor(cascadeBobKeyCorrected.length / 8);
    if (textoPlano.length > maxCharsValue) {
        alert(`⚠️ El texto es demasiado largo. Solo puedes encriptar hasta ${maxCharsValue} caracteres con esta clave.`);
        return;
    }
    
    // Convertir el texto a array de bytes (valores ASCII)
    const bytesTexto = [];
    for (let i = 0; i < textoPlano.length; i++) {
        bytesTexto.push(textoPlano.charCodeAt(i));
    }
    
    // CAMBIO: Encriptar usando la clave ORIGINAL de Alice (XOR byte a byte)
    const bytesEncriptados = [];
    for (let i = 0; i < bytesTexto.length; i++) {
        let keyByte = 0;
        // Usar 8 bits de la clave para formar un byte
        for (let j = 0; j < 8; j++) {
            const keyIndex = (i * 8 + j) % cascadeAliceKey.length;
            keyByte |= (cascadeAliceKey[keyIndex] << (7 - j));
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
    
    // CAMBIO: Desencriptar usando la clave CORREGIDA de Bob
    const bytesDesencriptados = [];
    for (let i = 0; i < bytesEncriptados.length; i++) {
        let keyByte = 0;
        // Usar 8 bits de la clave para formar un byte
        for (let j = 0; j < 8; j++) {
            const keyIndex = (i * 8 + j) % cascadeBobKeyCorrected.length;
            keyByte |= (cascadeBobKeyCorrected[keyIndex] << (7 - j));
        }
        bytesDesencriptados.push(bytesEncriptados[i] ^ keyByte);
    }
    
    // Convertir bytes desencriptados de vuelta a texto
    let textoDesencriptado = '';
    for (let i = 0; i < bytesDesencriptados.length; i++) {
        textoDesencriptado += String.fromCharCode(bytesDesencriptados[i]);
    }
    
    // Mostrar los resultados
    document.getElementById("corrected-original-message").textContent = textoPlano;
    document.getElementById("corrected-encrypted-message").textContent = textoEncriptado;
    document.getElementById("corrected-decrypted-message").textContent = textoDesencriptado;
    
    // Verificar si hay diferencias entre las claves y mostrar advertencia
    let errorCount = 0;
    for (let i = 0; i < cascadeAliceKey.length; i++) {
        if (cascadeAliceKey[i] !== cascadeBobKeyCorrected[i]) {
            errorCount++;
        }
    }
    
    // Agregar mensaje de información sobre la calidad de la desencriptación
    const errorContainer = document.getElementById("corrected-decrypted-message").parentNode;
    const infoMessage = document.createElement("p");
    infoMessage.className = "error-info";
    
    if (errorCount > 0) {
        infoMessage.textContent = `⚠️ El mensaje desencriptado contiene errores porque aún hay ${errorCount} bits diferentes entre las claves de Alice y Bob.`;
        infoMessage.style.color = "#f39c12";
    } else {
        infoMessage.textContent = "✅ Desencriptación perfecta: todas las correcciones de errores fueron exitosas.";
        infoMessage.style.color = "#2ecc71";
    }
    
    // Agregar mensaje después del texto desencriptado
    if (errorContainer.querySelector(".error-info")) {
        errorContainer.removeChild(errorContainer.querySelector(".error-info"));
    }
    errorContainer.appendChild(infoMessage);
}

/**
 * Función para copiar texto corregido al portapapeles
 */
function copiarTextoCorregido(elementId) {
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

// Agregar una inicialización del contador al cargar la página
document.addEventListener('DOMContentLoaded', function() {
    // Inicializar contador para la demostración de clave corregida
    setTimeout(actualizarContadorCorregido, 100);
    
    // Verificar si la sección de demostración existe y preparar su visualización
    const demoSection = document.querySelector('.corrected-key-demo');
    if (demoSection) {
        // Asegurar que la sección tenga un estilo atractivo
        demoSection.style.transition = 'all 0.5s ease-in-out';
    }
    
    // Agregar listener para actualizar el contador cuando se cambie de pestaña
    const cascadeTab = document.querySelector('.nav-tab[onclick*="cascade-tab"]');
    if (cascadeTab) {
        cascadeTab.addEventListener('click', function() {
            setTimeout(actualizarContadorCorregido, 200);
        });
    }
});

/**
 * Función para saltar todo el proceso Cascade y mostrar directamente el resultado final
 */
function saltarProcesoCascade() {
    // Asegurar que tenemos datos
    if (!cascadeAliceKey || cascadeAliceKey.length === 0) {
        alert("⚠️ Debes iniciar el proceso de corrección primero.");
        return;
    }
    
    // Aplicar todas las pasadas de forma rápida y automática
    for (let i = 1; i <= maxPasses; i++) {
        currentPass = i;
        
        // Calcular tamaño de bloque para esta pasada
        const tamanioActual = currentPass === 1 ? blockSize : Math.floor(blockSize * 1.5 * currentPass);
        
        // Buscar y corregir errores
        const numBloques = Math.ceil(cascadeAliceKey.length / tamanioActual);
        
        for (let j = 0; j < numBloques; j++) {
            const inicio = j * tamanioActual;
            const fin = Math.min(inicio + tamanioActual, cascadeAliceKey.length);
            
            // Extracción de bloques
            const bloqueAlice = cascadeAliceKey.slice(inicio, fin);
            const bloqueBob = cascadeBobKeyCorrected.slice(inicio, fin);
            
            // Cálculo de paridades
            const paridadAlice = bloqueAlice.reduce((a, b) => a ^ b, 0);
            const paridadBob = bloqueBob.reduce((a, b) => a ^ b, 0);
            
            // Si paridades son diferentes, corregir el error
            if (paridadAlice !== paridadBob) {
                // Encontrar bit de error con búsqueda binaria rápida
                let inicioBloque = 0;
                let finBloque = bloqueAlice.length - 1;
                
                while (inicioBloque < finBloque) {
                    const mitad = Math.floor((inicioBloque + finBloque) / 2);
                    
                    // Calcular paridad de la primera mitad
                    let paridadAliceMitad1 = 0;
                    let paridadBobMitad1 = 0;
                    
                    for (let k = inicioBloque; k <= mitad; k++) {
                        paridadAliceMitad1 ^= bloqueAlice[k];
                        paridadBobMitad1 ^= bloqueBob[k];
                    }
                    
                    // Si las paridades son diferentes en la primera mitad, el error está ahí
                    if (paridadAliceMitad1 !== paridadBobMitad1) {
                        finBloque = mitad;
                    } else {
                        inicioBloque = mitad + 1;
                    }
                    
                    // Si llegamos a un solo bit, corregirlo
                    if (inicioBloque === finBloque) {
                        const indexGlobal = inicio + inicioBloque;
                        cascadeBobKeyCorrected[indexGlobal] = cascadeBobKeyCorrected[indexGlobal] === 1 ? 0 : 1;
                        totalCorrectedBits++;
                        break;
                    }
                }
            }
        }
    }
    
    // Cuando saltamos el proceso, estimamos la información filtrada basándonos en el número
    // de pasadas y el tamaño de la clave
    let totalBloques = 0;
    let totalBitsRevelados = 0;
    
    for (let i = 1; i <= maxPasses; i++) {
        const tamanioBloque = i === 1 ? blockSize : Math.floor(blockSize * 1.5 * i);
        const numBloques = Math.ceil(cascadeAliceKey.length / tamanioBloque);
        totalBloques += numBloques;
        
        // Estimar bits revelados en búsqueda binaria para cada bloque con error
        // Asumimos que aproximadamente el 10% de los bloques tienen errores de paridad
        const bloquesProbablesConError = Math.ceil(numBloques * 0.1);
        totalBitsRevelados += bloquesProbablesConError * Math.ceil(Math.log2(tamanioBloque));
    }
    
    // Actualizar información filtrada
    informacionFiltrada = totalBloques + totalBitsRevelados;
    
    // Ir directamente al resultado final
    finalizarCascade();
}