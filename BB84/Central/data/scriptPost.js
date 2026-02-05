// Variables for the privacy amplification algorithm
let initialPrivacyKey = [];
let finalAmplifiedKey = [];
let hashMatrix = [];
let compressionRatio = 0.75;
let hashMethod = "toeplitz";

/**
 * Inicia el proceso de amplificación de privacidad
 */
function iniciarAmplificacion() {
    // Verificar si hay una clave corregida disponible
    if (!cascadeBobKeyCorrected || cascadeBobKeyCorrected.length === 0) {
        alert("⚠️ Debes corregir una clave primero con el algoritmo Cascade.");
        return;
    }
    
    // Obtener parámetros de configuración
    compressionRatio = parseFloat(document.getElementById("compression-ratio").value);
    hashMethod = document.getElementById("hash-method").value;
    
    // Inicializar variables
    initialPrivacyKey = [...cascadeBobKeyCorrected];
    
    // Calcular número de bits a extraer
    const bitsToExtract = Math.floor(initialPrivacyKey.length * compressionRatio);
    
    // Actualizar interfaz del paso 1
    document.querySelectorAll('.amplification-step').forEach(step => step.classList.remove('active'));
    document.getElementById('amplification-step1').classList.add('active');
    
    document.getElementById('initial-key-length').textContent = initialPrivacyKey.length;
    document.getElementById('bits-to-extract').textContent = bitsToExtract;
    
    // Visualizar la clave inicial
    visualizarClaveAmplificacion('initial-key', initialPrivacyKey);
}

/**
 * Avanza al siguiente paso en el proceso de amplificación
 */
function avanzarAmplificacion(paso) {
    document.querySelectorAll('.amplification-step').forEach(step => step.classList.remove('active'));
    document.getElementById(`amplification-step${paso}`).classList.add('active');
    
    if (paso === 2) {
        generarMatrizHash();
    } else if (paso === 3) {
        visualizarProcesoHash();
    } else if (paso === 4) {
        finalizarAmplificacion();
    }
}

/**
 * Genera la matriz hash para la amplificación de privacidad
 */
function generarMatrizHash() {
    const n = initialPrivacyKey.length;
    const m = Math.floor(n * compressionRatio);
    
    // Mostrar información de la matriz
    document.getElementById('hash-method-display').textContent = 
        hashMethod === "toeplitz" ? "Matriz Toeplitz" : "Función Universal-2";
    document.getElementById('matrix-dimensions').textContent = `${m} × ${n}`;
    
    // Generar matriz hash según el método seleccionado
    if (hashMethod === "toeplitz") {
        generarMatrizToeplitz(m, n);
    } else {
        generarMatrizUniversal2(m, n);
    }
    
    // Visualizar la matriz
    visualizarMatriz(hashMatrix, m, n);
}

/**
 * Genera una matriz Toeplitz para el hashing
 * Una matriz Toeplitz tiene elementos constantes en todas las diagonales descendentes de izquierda a derecha
 */
function generarMatrizToeplitz(m, n) {
    hashMatrix = [];
    
    // Generar vector de longitud (m + n - 1) con valores aleatorios binarios
    const vector = [];
    for (let i = 0; i < m + n - 1; i++) {
        vector.push(Math.round(Math.random()));
    }
    
    // Construir matriz Toeplitz a partir del vector
    for (let i = 0; i < m; i++) {
        const row = [];
        for (let j = 0; j < n; j++) {
            // El índice en el vector es la diferencia entre el índice de columna y el índice de fila más (n-1)
            row.push(vector[j - i + (n - 1)]);
        }
        hashMatrix.push(row);
    }
}

/**
 * Genera una matriz para la función universal 2
 * Es una matriz completamente aleatoria
 */
function generarMatrizUniversal2(m, n) {
    hashMatrix = [];
    
    // Generar matriz aleatoria de tamaño m x n
    for (let i = 0; i < m; i++) {
        const row = [];
        for (let j = 0; j < n; j++) {
            row.push(Math.round(Math.random()));
        }
        hashMatrix.push(row);
    }
}

/**
 * Visualiza la matriz hash en la interfaz
 */
function visualizarMatriz(matriz, filas, columnas) {
    const container = document.getElementById('matrix-container');
    container.innerHTML = '';
    
    // Establecer el estilo grid del contenedor
    container.style.gridTemplateColumns = `repeat(${columnas}, 20px)`;
    container.style.gridTemplateRows = `repeat(${filas}, 20px)`;
    
    // Crear celdas de la matriz
    for (let i = 0; i < filas; i++) {
        for (let j = 0; j < columnas; j++) {
            const cell = document.createElement('div');
            cell.className = `matrix-cell matrix-cell-${matriz[i][j]}`;
            cell.textContent = matriz[i][j];
            container.appendChild(cell);
        }
    }
}

/**
 * Visualiza el proceso de hash en la interfaz
 */
function visualizarProcesoHash() {
    // Mostrar clave original
    visualizarBitsEnLinea('hash-input-key', initialPrivacyKey);
    
    // Aplicar el hash
    finalAmplifiedKey = aplicarFuncionHash(initialPrivacyKey, hashMatrix);
    
    // Visualizar clave resultado del hash
    setTimeout(() => {
        visualizarBitsEnLinea('hash-output-key', finalAmplifiedKey);
    }, 1000); // Pequeño retraso para efecto visual
}

/**
 * Aplica la función hash a la clave original
 */
function aplicarFuncionHash(clave, matriz) {
    const m = matriz.length;
    const n = clave.length;
    const resultado = [];
    
    // Para cada fila de la matriz
    for (let i = 0; i < m; i++) {
        // Producto escalar de la fila con la clave (módulo 2)
        let bit = 0;
        for (let j = 0; j < n; j++) {
            bit ^= matriz[i][j] & clave[j];
        }
        resultado.push(bit);
    }
    
    return resultado;
}

/**
 * Visualiza bits en línea para el proceso de hash
 */
function visualizarBitsEnLinea(containerId, bits) {
    const container = document.getElementById(containerId);
    container.innerHTML = "";
    
    // Crear representación visual de bits
    bits.forEach(bit => {
        const bitElement = document.createElement("span");
        bitElement.className = `cascade-bit ${bit === 1 ? 'bit-1' : 'bit-0'}`;
        bitElement.textContent = bit;
        container.appendChild(bitElement);
    });
}

/**
 * Finaliza el proceso de amplificación y muestra los resultados
 */
function finalizarAmplificacion() {
    // Calcular estadísticas
    const originalLength = initialPrivacyKey.length;
    const finalLength = finalAmplifiedKey.length;
    const ratio = (finalLength / originalLength * 100).toFixed(2);
    
    // Calcular porcentaje de bits sacrificados
    const bitsSacrificados = (100 - parseFloat(ratio)).toFixed(2);
    
    // La entropía por bit aumenta con la reducción de longitud (esto es una aproximación simplificada)
    // En realidad, el cálculo de entropía es más complejo y depende de muchos factores
    const entropyPerBit = (1 / compressionRatio).toFixed(2);
    
    // Actualizar interfaz
    document.getElementById('original-length').textContent = originalLength;
    document.getElementById('final-length').textContent = finalLength;
    document.getElementById('sacrificed-bits-percentage').textContent = bitsSacrificados + "%";
    document.getElementById('entropy-per-bit').textContent = entropyPerBit;
    
    // Visualizar clave final
    visualizarClaveAmplificacion('final-amplified-key', finalAmplifiedKey);
    
    // Generar y mostrar clave hexadecimal
    const claveHex = bitsAHexadecimal(finalAmplifiedKey.map(bit => bit.toString()));
    document.getElementById('amplified-hex-key').textContent = claveHex;
    
    // Actualizar límite de caracteres para la demostración de encriptación
    actualizarLimiteCaracteresAmplificado();
}

/**
 * Visualiza una clave en la interfaz para amplificación
 */
function visualizarClaveAmplificacion(containerId, bitsClave) {
    const container = document.getElementById(containerId);
    container.innerHTML = "";
    
    for (let i = 0; i < bitsClave.length; i++) {
        const bit = bitsClave[i];
        
        const bitElement = document.createElement("span");
        bitElement.className = `cascade-bit ${bit === 1 ? 'bit-1' : 'bit-0'}`;
        bitElement.textContent = bit;
        container.appendChild(bitElement);
    }
}

/**
 * Copia la clave amplificada al portapapeles
 */
function copiarClaveAmplificada() {
    const claveHex = document.getElementById('amplified-hex-key').textContent;
    
    if (!claveHex || claveHex === '-') {
        alert("⚠️ No hay clave amplificada para copiar.");
        return;
    }
    
    navigator.clipboard.writeText(claveHex).then(() => {
        alert(`✅ Clave amplificada copiada al portapapeles: ${claveHex}`);
    }).catch(err => {
        console.error('Error al copiar: ', err);
        alert("❌ Error al copiar la clave. Intente nuevamente.");
    });
}

/**
 * Guarda la clave amplificada como archivo de texto
 */
function guardarClaveAmplificada() {
    const claveHex = document.getElementById('amplified-hex-key').textContent;
    
    if (!claveHex || claveHex === '-') {
        alert("⚠️ No hay clave amplificada para guardar.");
        return;
    }
    
    const contenido = `CLAVE BB84 AMPLIFICADA
Fecha: ${new Date().toLocaleString()}
Longitud original: ${document.getElementById('original-length').textContent} bits
Longitud final: ${document.getElementById('final-length').textContent} bits
Ratio de compresión: ${compressionRatio * 100}%
Entropía estimada por bit: ${document.getElementById('entropy-per-bit').textContent}
Método hash: ${document.getElementById('hash-method-display').textContent}

CLAVE AMPLIFICADA (HEX): ${claveHex}

CLAVE AMPLIFICADA (BITS): ${finalAmplifiedKey.join("")}
`;
    
    const blob = new Blob([contenido], { type: 'text/plain' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    
    a.href = url;
    a.download = `BB84_amplified_key_${timestamp}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    window.URL.revokeObjectURL(url);
    
    // Continuar al último paso para encriptación
    avanzarAmplificacion(5);
    
    alert("✅ Clave amplificada guardada correctamente.");
}

/**
 * Función para demostrar la encriptación y desencriptación usando la clave amplificada
 */
function demostrarEncriptacionAmplificada() {
    // Verificar si tenemos clave amplificada
    if (finalAmplifiedKey.length === 0) {
        alert("⚠️ Debes completar el proceso de amplificación de privacidad primero.");
        return;
    }
    
    // Obtener el texto a encriptar
    const textoPlano = document.getElementById("amplified-plain-text").value.trim();
    if (!textoPlano) {
        alert("⚠️ Introduce un texto para encriptar.");
        return;
    }
    
    // Verificar límite de caracteres
    const maxChars = parseInt(document.getElementById('amplified-max-chars').textContent);
    if (textoPlano.length > maxChars) {
        alert(`⚠️ El texto excede el límite de ${maxChars} caracteres. Por seguridad, la clave no debería reutilizarse demasiado.`);
        return;
    }
    
    // Si no hay suficientes bits en la clave
    if (finalAmplifiedKey.length < 8) {
        alert("⚠️ La clave amplificada es muy corta. Se necesitan al menos 8 bits para encriptar texto.");
        return;
    }
    
    // Convertir el texto a array de bytes (valores ASCII)
    const bytesTexto = [];
    for (let i = 0; i < textoPlano.length; i++) {
        bytesTexto.push(textoPlano.charCodeAt(i));
    }
    
    // Encriptar usando la clave amplificada (XOR byte a byte)
    const bytesEncriptados = [];
    for (let i = 0; i < bytesTexto.length; i++) {
        let keyByte = 0;
        // Usar 8 bits de la clave para formar un byte
        for (let j = 0; j < 8; j++) {
            const keyIndex = (i * 8 + j) % finalAmplifiedKey.length;
            keyByte |= (finalAmplifiedKey[keyIndex] << (7 - j));
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
    
    // Desencriptar usando la misma clave amplificada
    const bytesDesencriptados = [];
    for (let i = 0; i < bytesEncriptados.length; i++) {
        let keyByte = 0;
        // Usar 8 bits de la clave para formar un byte
        for (let j = 0; j < 8; j++) {
            const keyIndex = (i * 8 + j) % finalAmplifiedKey.length;
            keyByte |= (finalAmplifiedKey[keyIndex] << (7 - j));
        }
        bytesDesencriptados.push(bytesEncriptados[i] ^ keyByte);
    }
    
    // Convertir bytes desencriptados de vuelta a texto
    let textoDesencriptado = '';
    for (let i = 0; i < bytesDesencriptados.length; i++) {
        textoDesencriptado += String.fromCharCode(bytesDesencriptados[i]);
    }
    
    // Mostrar los resultados
    document.getElementById("amplified-original-message").textContent = textoPlano;
    document.getElementById("amplified-encrypted-message").textContent = textoEncriptado;
    document.getElementById("amplified-decrypted-message").textContent = textoDesencriptado;
}

/**
 * Función para copiar texto amplificado al portapapeles
 */
function copiarTextoAmplificado(elementId) {
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

// Character counting functions
/**
 * Calcula y muestra el número máximo de caracteres que se pueden encriptar con la clave original
 */
function actualizarLimiteCaracteresOriginal() {
    // Cada caracter necesita 8 bits para ser encriptado
    // La clave se puede reutilizar cíclicamente, pero para mantener seguridad
    // no deberíamos usar más de la longitud de la clave dividida por 8
    let maxChars = Math.floor(bitsClave.length / 8);
    
    // Asegurar un valor mínimo
    maxChars = Math.max(1, maxChars);
    
    // Actualizar la interfaz
    document.getElementById('max-chars').textContent = maxChars;
    
    // Actualizar contador con el número actual de caracteres
    const charCount = document.getElementById('plain-text').value.length;
    document.getElementById('char-count').textContent = charCount;
    
    // Resaltar si se excede el límite
    const container = document.getElementById('plain-text').parentNode;
    if (charCount > maxChars) {
        container.classList.add('character-limit-exceeded');
    } else {
        container.classList.remove('character-limit-exceeded');
    }
}

/**
 * Actualiza el contador de caracteres para la demostración de encriptación original
 */
function actualizarContadorOriginal() {
    if (!bitsClave || bitsClave.length === 0) return;
    
    const textLength = document.getElementById('plain-text').value.length;
    document.getElementById('char-count').textContent = textLength;
    
    const maxChars = parseInt(document.getElementById('max-chars').textContent);
    const container = document.getElementById('plain-text').parentNode;
    
    if (textLength > maxChars) {
        container.classList.add('character-limit-exceeded');
    } else {
        container.classList.remove('character-limit-exceeded');
    }
}

/**
 * Calcula y muestra el número máximo de caracteres que se pueden encriptar con la clave amplificada
 */
function actualizarLimiteCaracteresAmplificado() {
    // Cada caracter necesita 8 bits para ser encriptado
    let maxChars = Math.floor(finalAmplifiedKey.length / 8);
    
    // Asegurar un valor mínimo
    maxChars = Math.max(1, maxChars);
    
    // Actualizar la interfaz
    document.getElementById('amplified-max-chars').textContent = maxChars;
    
    // Actualizar contador con el número actual de caracteres
    const charCount = document.getElementById('amplified-plain-text').value.length;
    document.getElementById('amplified-char-count').textContent = charCount;
    
    // Resaltar si se excede el límite
    const container = document.getElementById('amplified-plain-text').parentNode;
    if (charCount > maxChars) {
        container.classList.add('character-limit-exceeded');
    } else {
        container.classList.remove('character-limit-exceeded');
    }
}

/**
 * Actualiza el contador de caracteres para la demostración de encriptación con clave amplificada
 */
function actualizarContadorAmplificado() {
    if (!finalAmplifiedKey || finalAmplifiedKey.length === 0) return;
    
    const textLength = document.getElementById('amplified-plain-text').value.length;
    document.getElementById('amplified-char-count').textContent = textLength;
    
    const maxChars = parseInt(document.getElementById('amplified-max-chars').textContent);
    const container = document.getElementById('amplified-plain-text').parentNode;
    
    if (textLength > maxChars) {
        container.classList.add('character-limit-exceeded');
    } else {
        container.classList.remove('character-limit-exceeded');
    }
}

// Event listener to initialize character counters when the page loads
document.addEventListener('DOMContentLoaded', function() {
    // Initialize character counters
    document.getElementById('char-count').textContent = '0';
    document.getElementById('max-chars').textContent = '0';
    document.getElementById('amplified-char-count').textContent = '0';
    document.getElementById('amplified-max-chars').textContent = '0';
});
