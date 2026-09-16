# Project48 Oracle ARM64 서버 배포 가이드

이 문서는 `Project48Server-LinuxArm64.tar.gz`를 전달받은 서버 담당자가 Oracle Cloud의 Linux ARM64 서버에 Project48 전용 서버를 설치하고 운영하는 절차입니다.

## 1. 배포 환경

- Oracle Cloud Shape: `VM.Standard.A1.Flex`
- CPU Architecture: `ARM64` / `aarch64`
- 서버 공인 IP: `152.67.221.82`
- 로비 서버: UDP `17777`
- 게임 서버 1: UDP `17778`
- 게임 서버 2: UDP `17779`
- 게임 서버 3: UDP `17780`
- 게임 서버 보고 수신기: TCP `17776` — 동일 VM 내부에서만 사용하며 외부에 개방하지 않습니다.

배포 파일:

```text
Project48Server-LinuxArm64.tar.gz
```

## 2. OCI 네트워크 규칙

인스턴스가 연결된 Network Security Group 또는 Subnet Security List에 Stateful Ingress 규칙을 추가합니다.

```text
Source CIDR:       0.0.0.0/0
IP Protocol:       UDP
Destination Port:  17777-17780
```

SSH 포트 `22`는 가능하면 관리자 IP에서만 접근하도록 제한합니다.

TCP `17776`은 외부에 개방하지 않습니다. 게임 서버 프로세스는 `127.0.0.1:17776`으로 로비에 상태를 보고합니다.

## 3. Linux 방화벽

Oracle Linux에서 `firewalld`를 사용하는 경우:

```bash
sudo firewall-cmd --permanent --add-port=17777-17780/udp
sudo firewall-cmd --reload
sudo firewall-cmd --list-ports
```

Ubuntu에서 `ufw`를 사용하는 경우:

```bash
sudo ufw allow 17777:17780/udp
sudo ufw status
```

## 4. 전용 사용자 및 설치 폴더 생성

서비스를 `root`로 실행하지 않습니다.

```bash
sudo useradd --system --home-dir /opt/project48 --shell /sbin/nologin project48 2>/dev/null || true
sudo mkdir -p /opt/project48
```

전달받은 압축 파일을 `/tmp/Project48Server-LinuxArm64.tar.gz`에 업로드한 뒤 압축을 해제합니다.

```bash
sudo tar -xzf /tmp/Project48Server-LinuxArm64.tar.gz \
  -C /opt/project48 \
  --strip-components=1

sudo chown -R project48:project48 /opt/project48
sudo chmod +x /opt/project48/Project48Server-Arm64.sh
sudo chmod +x /opt/project48/Project48/Binaries/LinuxArm64/Project48Server
```

실행 파일 확인:

```bash
sudo -u project48 test -x /opt/project48/Project48Server-Arm64.sh
sudo -u project48 test -x /opt/project48/Project48/Binaries/LinuxArm64/Project48Server
uname -m
```

`uname -m` 출력은 `aarch64`여야 합니다.

SHA-256 해시를 함께 전달받았다면 압축 해제 전에 확인합니다.

```bash
sha256sum /tmp/Project48Server-LinuxArm64.tar.gz
```

## 5. 운영용 보고 토큰 생성

로비 서버와 게임 서버는 동일한 비밀 토큰을 사용해야 합니다. 토큰을 소스 코드나 서비스 파일에 직접 기록하지 않습니다.

```bash
sudo mkdir -p /etc/project48
TOKEN="$(openssl rand -hex 32)"
sudo sh -c "printf '%s\n' 'P48_REPORT_TOKEN=${TOKEN}' > /etc/project48/project48.env"
unset TOKEN
sudo chown root:root /etc/project48/project48.env
sudo chmod 600 /etc/project48/project48.env
```

이 파일은 외부에 전달하거나 Git에 커밋하지 않습니다.

## 6. 로비 서버 systemd 서비스

다음 파일을 생성합니다.

```bash
sudo tee /etc/systemd/system/project48-lobby.service >/dev/null <<'EOF'
[Unit]
Description=Project48 Lobby Server
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
User=project48
Group=project48
WorkingDirectory=/opt/project48
EnvironmentFile=/etc/project48/project48.env
ExecStart=/opt/project48/Project48Server-Arm64.sh /Game/WJS/Lobby/L_Lobby -server -unattended -log -port=17777 -NicknameDatabaseServer -LobbyGameServerAddresses=152.67.221.82:17778,152.67.221.82:17779,152.67.221.82:17780 -LobbyReturnAddress=152.67.221.82:17777 -LobbyGameServerReportPort=17776 -GameServerReportToken=${P48_REPORT_TOKEN}
Restart=on-failure
RestartSec=3
TimeoutStopSec=20
LimitNOFILE=65535
UMask=0027

[Install]
WantedBy=multi-user.target
EOF
```

## 7. 게임 서버 systemd 템플릿

포트 `17778`, `17779`, `17780`에서 동일한 게임 서버를 실행하기 위한 템플릿입니다.

```bash
sudo tee /etc/systemd/system/project48-game@.service >/dev/null <<'EOF'
[Unit]
Description=Project48 Game Server on UDP %i
Wants=network-online.target project48-lobby.service
After=network-online.target project48-lobby.service

[Service]
Type=simple
User=project48
Group=project48
WorkingDirectory=/opt/project48
EnvironmentFile=/etc/project48/project48.env
ExecStart=/opt/project48/Project48Server-Arm64.sh /Game/MSJ/Maps/P48_FlyingIslandMap?game=/Game/KSH/Game/BP_P48SurvivalGameMode.BP_P48SurvivalGameMode_C -server -unattended -log -port=%i -GameServerReportUrl=http://127.0.0.1:17776/game-server/report -GameServerReportToken=${P48_REPORT_TOKEN}
Restart=on-failure
RestartSec=3
TimeoutStopSec=20
LimitNOFILE=65535
UMask=0027

[Install]
WantedBy=multi-user.target
EOF
```

운영 환경에서는 `-LocalServerTest` 옵션을 사용하지 않습니다. 이 옵션을 사용하면 게임 서버의 `match_ended` 및 `server_ready` 보고가 비활성화됩니다.

## 8. 서비스 등록 및 실행

```bash
sudo systemctl daemon-reload

sudo systemctl enable --now project48-lobby.service
sudo systemctl enable --now project48-game@17778.service
sudo systemctl enable --now project48-game@17779.service
sudo systemctl enable --now project48-game@17780.service
```

서비스 상태를 확인합니다.

```bash
sudo systemctl status project48-lobby.service --no-pager
sudo systemctl status project48-game@17778.service --no-pager
sudo systemctl status project48-game@17779.service --no-pager
sudo systemctl status project48-game@17780.service --no-pager
```

모든 서비스가 `active (running)`이어야 합니다.

## 9. 포트와 로그 확인

UDP 게임 포트:

```bash
sudo ss -lunp | grep -E '17777|17778|17779|17780'
```

로컬 HTTP 보고 포트:

```bash
sudo ss -ltnp | grep 17776
```

실시간 로비 로그:

```bash
sudo journalctl -u project48-lobby.service -f
```

실시간 게임 서버 로그:

```bash
sudo journalctl -u project48-game@17778.service -f
```

최근 로그를 한 번에 확인하려면:

```bash
sudo journalctl -u project48-lobby.service -n 200 --no-pager
sudo journalctl -u 'project48-game@*.service' -n 300 --no-pager
```

정상 실행 시 다음 내용을 확인합니다.

- 로비 서버가 UDP `17777`에서 실행 중
- 게임 서버가 UDP `17778-17780`에서 실행 중
- 로비가 TCP `17776`에서 게임 서버 보고를 수신 중
- 게임 서버 로그에 보고 엔드포인트 설정 오류가 없음
- 클라이언트의 로비 복귀 주소가 `152.67.221.82:17777`로 전달됨

## 10. 클라이언트 접속 테스트

Windows 클라이언트는 다음 주소로 접속합니다.

```text
152.67.221.82:17777
```

권장 테스트 순서:

1. 클라이언트 2대 이상으로 로비 접속
2. 방 생성 및 준비 완료
3. 게임 서버 `17778-17780` 중 하나로 이동하는지 확인
4. PCG 맵 생성 및 경기 시작 확인
5. 경기 종료 후 `152.67.221.82:17777`로 복귀하는지 확인
6. 게임 서버 로그에서 `match_ended`와 `server_ready` 보고 확인
7. 같은 게임 서버가 다음 매치에 다시 배정되는지 확인

## 11. 서비스 재시작 및 중지

전체 재시작:

```bash
sudo systemctl restart project48-lobby.service
sudo systemctl restart project48-game@17778.service
sudo systemctl restart project48-game@17779.service
sudo systemctl restart project48-game@17780.service
```

전체 중지:

```bash
sudo systemctl stop project48-game@17778.service
sudo systemctl stop project48-game@17779.service
sudo systemctl stop project48-game@17780.service
sudo systemctl stop project48-lobby.service
```

코드에는 서버 맵 재로드 실패 시 비정상 종료하는 복구 경로가 있습니다. `Restart=on-failure`를 제거하면 이 복구가 작동하지 않으므로 반드시 유지합니다.

## 12. 새 버전 업데이트

닉네임 DB와 런타임 데이터를 보존하기 위해 `Project48/Saved`를 백업합니다.

```bash
sudo systemctl stop project48-game@17778.service
sudo systemctl stop project48-game@17779.service
sudo systemctl stop project48-game@17780.service
sudo systemctl stop project48-lobby.service

sudo mkdir -p /var/backups/project48
sudo tar -czf "/var/backups/project48/Saved-$(date +%Y%m%d-%H%M%S).tar.gz" \
  -C /opt/project48/Project48 Saved
```

새 압축 파일을 기존 경로에 덮어씁니다. `Saved` 폴더를 삭제하지 않습니다.

```bash
sudo tar -xzf /tmp/Project48Server-LinuxArm64.tar.gz \
  -C /opt/project48 \
  --strip-components=1

sudo chown -R project48:project48 /opt/project48
sudo chmod +x /opt/project48/Project48Server-Arm64.sh
sudo chmod +x /opt/project48/Project48/Binaries/LinuxArm64/Project48Server

sudo systemctl start project48-lobby.service
sudo systemctl start project48-game@17778.service
sudo systemctl start project48-game@17779.service
sudo systemctl start project48-game@17780.service
```

업데이트 후 서비스 상태와 로그를 다시 확인합니다.

## 13. 문제 해결

### `Exec format error`

서버가 `aarch64`인지, 전달받은 파일이 Linux ARM64 패키지인지 확인합니다.

```bash
uname -m
file /opt/project48/Project48/Binaries/LinuxArm64/Project48Server
```

### `Permission denied`

```bash
sudo chown -R project48:project48 /opt/project48
sudo chmod +x /opt/project48/Project48Server-Arm64.sh
sudo chmod +x /opt/project48/Project48/Binaries/LinuxArm64/Project48Server
```

### 클라이언트가 로비에 접속하지 못함

- OCI NSG 또는 Security List에 UDP `17777`이 열려 있는지 확인
- Linux 방화벽에 UDP `17777`이 열려 있는지 확인
- `project48-lobby.service` 상태 확인
- 클라이언트가 `152.67.221.82:17777`로 접속하는지 확인

### 로비에서 게임 서버로 이동하지 못함

- UDP `17778-17780` 개방 확인
- 게임 서버 3개의 서비스 상태 확인
- 로비의 `LobbyGameServerAddresses` 인자 확인
- 게임 서버 로그의 `GameServerReportUrl` 및 토큰 오류 확인

### 게임 종료 후 서버가 다시 사용 가능해지지 않음

- 게임 서버 로그에서 `match_ended`, `server_ready` 확인
- 로비 로그에서 게임 서버 보고 수신 확인
- TCP `17776`을 다른 프로세스가 사용하고 있지 않은지 확인
- `Restart=on-failure` 설정 확인

### 닉네임 DB 위치

```text
/opt/project48/Project48/Saved/Database/Nickname.db
```

이 파일과 전체 `Project48/Saved` 폴더를 정기적으로 백업합니다.
