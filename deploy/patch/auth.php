<?php
declare(strict_types=1);

/*
 * Ponte de autenticação para o Trickster Launcher.
 *
 * Contrato recebido:
 *   POST form / JSON: username, password (opcional: auth_token)
 *
 * Contrato devolvido:
 *   {"status":"success","message":"Login success"}
 *   ou {"status":"error","message":"..."}
 *
 * Encaminha as credenciais com cabeçalhos de segurança (X-Real-IP, X-Request-ID)
 * diretamente para a API centralizada na VPS.
 */

const DEFAULT_API_URL = 'http://15.235.173.71:8000/api/login';

header('Content-Type: application/json; charset=utf-8');

function reply(array $payload): void
{
    http_response_code(200);
    echo json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    reply(['status' => 'error', 'message' => 'Invalid request method.']);
}

// Suporte a form-urlencoded e payload JSON
$username = $_POST['username'] ?? null;
$password = $_POST['password'] ?? null;

if ($username === null || $password === null) {
    $rawInput = file_get_contents('php://input');
    if ($rawInput !== false && !empty($rawInput)) {
        $json = json_decode($rawInput, true);
        if (is_array($json)) {
            $username = $json['username'] ?? null;
            $password = $json['password'] ?? null;
        }
    }
}

if (!is_string($username) || !is_string($password)) {
    reply(['status' => 'error', 'message' => 'Login failed']);
}

$username = trim($username);
if ($username === '' || strlen($username) > 18 || strlen($password) > 128) {
    reply(['status' => 'error', 'message' => 'Login failed']);
}

if (!function_exists('curl_init')) {
    error_log('auth.php: PHP cURL extension is not enabled');
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
        'X-Real-IP: ' . $clientIp,
    ],
    CURLOPT_RETURNTRANSFER => true,
    CURLOPT_CONNECTTIMEOUT => 4,
    CURLOPT_TIMEOUT => 15,
    CURLOPT_FOLLOWLOCATION => false,
]);

$rawResponse = curl_exec($curl);
$curlError = curl_error($curl);
$httpCode = (int) curl_getinfo($curl, CURLINFO_HTTP_CODE);
curl_close($curl);

if ($rawResponse === false) {
    error_log('auth.php: API request failed: ' . $curlError);
    reply(['status' => 'error', 'message' => 'Login service unavailable.']);
}

$apiResponse = json_decode($rawResponse, true);
if (!is_array($apiResponse)) {
    error_log('auth.php: API returned invalid JSON');
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

// 401 e demais respostas de validação
reply(['status' => 'error', 'message' => 'Incorrect username/password.']);
