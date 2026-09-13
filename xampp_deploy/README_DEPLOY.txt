========================================================================
 GUIA DE DEPLOY NO XAMPP (APACHE / PHP) - TRICKSTER LAUNCHER
========================================================================

1. ESTRUTURA AUTOMATIZADA:
   - O launcher resolve automaticamente o endpoint de autenticação a partir
     da URL do CDN: {scheme}://{cdn}/auth.php
   - O token de segurança é recuperado automaticamente do CDN (auth_token.txt / auth_token.enc).
   - Nenhuma URL de autenticação ou token precisa ser configurada manualmente no cliente.

2. COMO INSTALAR:
   - Extraia a pasta "patch" deste pacote diretamente dentro de:
     C:\xampp\htdocs\
     
   - A estrutura no XAMPP ficará:
     C:\xampp\htdocs\patch\
       ├── FileListGen.exe         (Gerador de manifestos e launcher.txt)
       ├── Splash.exe              (Binário do Launcher para autoupdate)
       ├── launcher.txt            (Hash SHA-256 do Splash.exe)
       ├── manifest.json           (Lista de arquivos e versões do patch)
       ├── auth.php                (Endpoint de autenticação de login)
       ├── auth_token.txt          (Token de autenticação do CDN)
       └── Trickster/
           └── trickster.bin       (Arquivos do cliente do jogo)

3. INICIAR O APACHE NO XAMPP:
   - Abra o "XAMPP Control Panel".
   - Clique em "Start" no módulo "Apache".
   - Para testar no navegador:
     http://127.0.0.1/patch/auth_token.txt
     http://127.0.0.1/patch/manifest.json

4. GERAR NOVAS ATUALIZAÇÕES:
   - Sempre que adicionar ou modificar arquivos do jogo na pasta "Trickster/",
     ou atualizar o Splash.exe, abra o terminal dentro de "C:\xampp\htdocs\patch"
     e execute simplesmente:
     .\FileListGen.exe
     
   - O FileListGen irá escanear a pasta "Trickster/", gerar o "manifest.json"
     com os caminhos relativos e atualizar o "launcher.txt".

5. CONFIGURAÇÃO NO LAUNCHER DO CLIENTE (LauncherData/config.json):
   {
     "window_title": "Trickster Online",
     "subtitle": "Game Launcher",
     "cdn": "127.0.0.1/patch",
     "use_ssl": false,
     "option_exec": "apps/Setup.exe",
     "game_exec": "Trickster/trickster.bin",
     "region": "thailand"
   }
========================================================================
