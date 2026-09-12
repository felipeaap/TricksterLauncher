<?php
/**
 * launcher_auth.php
 *
 * Standalone PHP script implementing a custom authentication protocol for the
 * Trickster Launcher client (C++). It receives a JSON payload with credentials,
 * validates them against a MySQL database, and returns a JSON response with a
 * session token.
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

// Shared secret for HMAC signature verification – change to a strong secret.
define('SHARED_SECRET', 'replace_this_with_a_strong_secret');

// Database connection configuration – replace with actual credentials.
define('DB_DSN', 'mysql:host=127.0.0.1;dbname=launcher_db;charset=utf8mb4');
define('DB_USER', 'launcher_user');
define('DB_PASS', 'launcher_password');

function respond(string $status, string $message, ?string $token = null): void {
    $response = ['status' => $status, 'message' => $message];
    if ($token !== null) {
        $response['token'] = $token;
    }
    echo json_encode($response);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    respond('error', 'Only POST requests are allowed.');
}

$rawInput = file_get_contents('php://input');
if ($rawInput === false || empty($rawInput)) {
    respond('error', 'Empty request body.');
}

$payload = json_decode($rawInput, true);
if (json_last_error() !== JSON_ERROR_NONE) {
    respond('error', 'Invalid JSON payload: ' . json_last_error_msg());
}

$required = ['username', 'password', 'signature'];
foreach ($required as $field) {
    if (empty($payload[$field])) {
        respond('error', "Missing required field: $field");
    }
}

$nonce = $payload['nonce'] ?? '';
$signedData = json_encode([
    'username' => $payload['username'],
    'password' => $payload['password'],
    'nonce'    => $nonce
]);
$expectedSignature = hash_hmac('sha256', $signedData, SHARED_SECRET);
if (!hash_equals($expectedSignature, $payload['signature'])) {
    respond('error', 'Invalid signature.');
}

try {
    $pdo = new PDO(DB_DSN, DB_USER, DB_PASS, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC
    ]);
} catch (PDOException $e) {
    respond('error', 'Database connection failed: ' . $e->getMessage());
}

$stmt = $pdo->prepare('SELECT id, password_hash FROM users WHERE username = :u');
$stmt->execute([':u' => $payload['username']]);
$user = $stmt->fetch();
if (!$user || !password_verify($payload['password'], $user['password_hash'])) {
    respond('error', 'Invalid credentials.');
}

$sessionToken = bin2hex(random_bytes(32));
try {
    $stmt = $pdo->prepare('INSERT INTO sessions (user_id, token, created_at) VALUES (:uid, :tok, NOW())');
    $stmt->execute([':uid' => $user['id'], ':tok' => $sessionToken]);
} catch (PDOException $e) {
    // If sessions table does not exist, ignore; token is still returned.
}

respond('ok', 'Authentication successful.', $sessionToken);
?>
