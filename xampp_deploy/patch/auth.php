<?php
/**
 * auth.php - Trickster Online Launcher Authentication Endpoint
 * Compatible with Apache/PHP (XAMPP).
 */

header('Content-Type: application/json; charset=utf-8');

// Lê o token esperado diretamente de auth_token.txt se existir
$tokenFile = __DIR__ . '/auth_token.txt';
$expectedServerToken = file_exists($tokenFile) ? trim(file_get_contents($tokenFile)) : '';

// Recebe dados do formulário (POST x-www-form-urlencoded)
$authToken = $_POST['auth_token'] ?? '';
$username  = $_POST['username'] ?? '';
$password  = $_POST['password'] ?? '';

// 1. Valida o Token do Servidor apenas se um token foi configurado no servidor em auth_token.txt
if (!empty($expectedServerToken) && $authToken !== $expectedServerToken) {
    http_response_code(403);
    echo json_encode([
        'status'  => 'error',
        'message' => 'Token de autenticação do launcher inválido ou ausente.'
    ]);
    exit;
}

// 2. Valida se os campos de usuário e senha foram preenchidos
if (empty($username) || empty($password)) {
    http_response_code(400);
    echo json_encode([
        'status'  => 'error',
        'message' => 'Por favor, informe o usuário e a senha.'
    ]);
    exit;
}

// 3. Validação de credenciais (Mock para testes ou integração com banco de dados)
// Exemplo de integração com MySQL do XAMPP:
/*
try {
    $pdo = new PDO("mysql:host=127.0.0.1;dbname=trickster_db;charset=utf8mb4", "root", "");
    $stmt = $pdo->prepare("SELECT id, password_hash FROM accounts WHERE username = :u");
    $stmt->execute([':u' => $username]);
    $acc = $stmt->fetch(PDO::FETCH_ASSOC);
    if (!$acc || !password_verify($password, $acc['password_hash'])) {
        http_response_code(401);
        echo json_encode(['status' => 'error', 'message' => 'Usuário ou senha incorretos.']);
        exit;
    }
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => 'Erro ao conectar ao banco de dados: ' . $e->getMessage()]);
    exit;
}
*/

// Sucesso no teste: gera um token de sessão
$sessionToken = bin2hex(random_bytes(16));

echo json_encode([
    'status'  => 'ok',
    'message' => 'Login realizado com sucesso!',
    'token'   => $sessionToken
]);
exit;
