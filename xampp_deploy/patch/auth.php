<?php
declare(strict_types=1);

/*
 * Ponte de compatibilidade para o launcher legado.
 *
 * Contrato recebido:
 *   POST form: auth_token, username, password
 *
 * Contrato devolvido:
 *   {"status":"success","message":"Login success"}
 *   ou {"status":"error","message":"..."}
 *
 * Este arquivo NÃO acessa SQL Server. A única conexão é com a API unificada
 * na VPS nova. O token é validado localmente e nunca é encaminhado à API.
 */

const DEFAULT_API_URL = 'http://15.235.173.71:8000/api/login';

header('Content-Type: application/json; charset=utf-8');

function reply(array $payload)
{
    // O endpoint PHP original devolvia erros de autenticação com HTTP 200;
    // manter isso evita quebrar launchers antigos que leem apenas o JSON.
    http_response_code(200);
    echo json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

function configured_token(): string
{
    $environmentToken = getenv('LAUNCHER_AUTH_TOKEN');
    if (is_string($environmentToken) && $environmentToken !== '') {
        return trim($environmentToken);
    }

    // Fora do document root. Exemplo: C:\\xampp\\private\\launcher-auth.secret
    $secretFile = getenv('LAUNCHER_AUTH_SECRET_FILE');
    if (!is_string($secretFile) || $secretFile === '') {
        $secretFile = dirname(__DIR__, 2) . DIRECTORY_SEPARATOR . 'private'
            . DIRECTORY_SEPARATOR . 'launcher-auth.secret';
    }

    if (!is_file($secretFile) || !is_readable($secretFile)) {
        return '';
    }

    $contents = file_get_contents($secretFile);
    return is_string($contents) ? trim($contents) : '';
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    reply(['status' => 'error', 'message' => 'Invalid request method.']);
}

$receivedToken = $_POST['auth_token'] ?? '';
$expectedToken = configured_token();
if (!is_string($receivedToken) || $expectedToken === ''
    || !hash_equals($expectedToken, $receivedToken)) {
    reply(['status' => 'error', 'message' => 'No authority!']);
}

$username = $_POST['username'] ?? null;
$password = $_POST['password'] ?? null;
if (!is_string($username) || !is_string($password)) {
    reply(['status' => 'error', 'message' => 'Login failed']);
}

$username = trim($username);
if ($username === '' || strlen($username) > 18 || strlen($password) > 128) {
    reply(['status' => 'error', 'message' => 'Login failed']);
}

if (!function_exists('curl_init')) {
    error_log('launcher_auth.php: PHP cURL extension is not enabled');
    reply(['status' => 'error', 'message' => 'Login service unavailable.']);
}

$apiUrl = getenv('TRICKSTER_API_LOGIN_URL');
if (!is_string($apiUrl) || $apiUrl === '') {
    $apiUrl = DEFAULT_API_URL;
}

$requestBody = json_encode(
    ['username' => $username, 'password' => $password],
    JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES,
);
if ($requestBody === false) {
    reply(['status' => 'error', 'message' => 'Login failed']);
}

$clientIp = $_SERVER['REMOTE_ADDR'] ?? '';
$requestId = bin2hex(random_bytes(16));
$curl = curl_init($apiUrl);
curl_setopt_array($curl, [
    CURLOPT_POST => true,
    CURLOPT_POSTFIELDS => $requestBody,
    CURLOPT_HTTPHEADER => [
        'Accept: application/json',
        'Content-Type: application/json',
        'X-Request-ID: ' . $requestId,
        // O API usa este valor somente para rate limiting. Ele é preenchido
        // pelo servidor, nunca por um header recebido do cliente.
        'X-Real-IP: ' . $clientIp,
    ],
    CURLOPT_RETURNTRANSFER => true,
    CURLOPT_CONNECTTIMEOUT => 3,
    CURLOPT_TIMEOUT => 15,
    CURLOPT_FOLLOWLOCATION => false,
]);

$rawResponse = curl_exec($curl);
$curlError = curl_error($curl);
$httpCode = (int) curl_getinfo($curl, CURLINFO_HTTP_CODE);
curl_close($curl);

if ($rawResponse === false) {
    error_log('launcher_auth.php: API request failed: ' . $curlError);
    reply(['status' => 'error', 'message' => 'Login service unavailable.']);
}

$apiResponse = json_decode($rawResponse, true);
if (!is_array($apiResponse)) {
    error_log('launcher_auth.php: API returned invalid JSON');
    reply(['status' => 'error', 'message' => 'Login service unavailable.']);
}

if ($httpCode === 200 && ($apiResponse['success'] ?? false) === true) {
    reply(['status' => 'success', 'message' => 'Login success']);
}

$detail = $apiResponse['detail'] ?? null;
if ($httpCode === 403 && is_array($detail)) {
    reply([
        'status' => 'error',
        'message' => 'Account is banned.',
        'release_date' => $detail['release_date'] ?? null,
        'reason' => $detail['reason'] ?? null,
    ]);
}

if ($httpCode === 409) {
    reply(['status' => 'error', 'message' => 'Player is already online.']);
}

if ($httpCode === 429) {
    reply(['status' => 'error', 'message' => 'Too many login attempts.']);
}

if ($httpCode >= 500) {
    reply(['status' => 'error', 'message' => 'Login service unavailable.']);
}

// 401 e demais respostas de validação continuam genéricas para não revelar
// se o usuário existe no banco.
reply(['status' => 'error', 'message' => 'Incorrect username/password.']);
