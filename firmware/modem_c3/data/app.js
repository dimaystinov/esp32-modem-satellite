const $ = (id) => document.getElementById(id);

const ui = {
  stamp: $("stamp"),
  status: $("status"),
  mission: $("mission"),
  protocol: $("protocol"),
  logs: $("logs"),
  raw: $("raw"),
  btnUpload: $("btn-upload"),
  btnStop: $("btn-stop"),
  btnEcho: $("btn-echo"),
  btnRaw: $("btn-raw"),
  btnMissionClear: $("btn-mission-clear"),
  btnClear: $("btn-clear"),
};

let echoEnabled = false;
let wifiDeadline = null;
function updateCountdown() {
  if(wifiDeadline===null) return;
  const sec=Math.max(0,Math.ceil((wifiDeadline-performance.now())/1000));
  $("wifi-countdown").textContent=sec ? "Wi-Fi отключится через " + Math.floor(sec/60) + ":" + String(sec%60).padStart(2,"0") : "Время Wi-Fi истекло. Отображаемые данные больше не обновляются.";
}
setInterval(updateCountdown,250);

function nowStamp() {
  const d = new Date();
  return d.toLocaleString();
}

function bool(v) {
  return v ? "YES" : "NO";
}

async function request(path, opts = {}) {
  const res = await fetch(path, {signal: AbortSignal.timeout(4000), ...opts});
  const txt = await res.text();
  if (!res.ok) throw new Error(`${res.status}: ${txt}`);
  if (!txt) return null;
  const c = res.headers.get("content-type") || "";
  return c.includes("application/json") ? JSON.parse(txt) : txt;
}

function renderStatus(j) {
  $("switch-state").textContent = (j.switch.enabled ? "Включён" : "Выключен") + " · GPIO4 " + (j.switch.level ? "HIGH" : "LOW");
  const names = {waiting_mission:"Ожидание нового задания: питание выключено",waiting_heartbeat:"Питание включено: ожидание heartbeat полётника",uploading:"Загрузка миссии в полётник",complete:"Полётник подтвердил приём миссии",error:"Ошибка загрузки: питание остаётся включённым",cancelled:"Загрузка отменена: питание остаётся включённым"};
  $("sequence").textContent = [names[j.sequence.stage] || j.sequence.stage,
    "Задание принято в этом запуске: " + bool(j.sequence.acceptedThisBoot),
    "Питание включено: " + Math.floor(j.sequence.poweredForMs/1000) + " с",
    "Связь с полётником: " + (j.mavlink.connected ? "есть" : "нет"),
    "Принято байтов UART от полётника: " + j.mavlink.rxBytes,
    "Возраст heartbeat: " + (j.mavlink.heartbeatCount ? j.mavlink.heartbeatAgeMs + " мс" : "ещё не получен"),
    j.sequence.waitingLong ? "Полётник не ответил за 30 с. Ожидание продолжается." : "",
    "Свободная память ESP: " + j.freeHeap + " байт"].join("\n");
  $("wifi-details").textContent = j.wifi.ssid + " · клиентов: " + j.wifi.clients + " · DNS: " + (j.wifi.dns_enabled ? j.wifi.dns_name : "недоступен");
  wifiDeadline = performance.now() + j.wifi.remaining_ms;
  updateCountdown();
  ui.btnUpload.disabled = !j.sequence.acceptedThisBoot || !j.mavlink.connected || j.sequence.stage === "uploading";
  ui.stamp.textContent = nowStamp();

  ui.status.textContent = [
    `Mode: ${(j.runtime?.mode ?? "-").toUpperCase()}`,
    `USB serial connected: ${bool(j.runtime?.usbSerialConnected)}`,
    `Protocol transport: ${j.runtime?.protocolTransport ?? "-"}`,
    `Protocol RX/TX pins: ${j.runtime?.protocolRxPin ?? "-"}/${j.runtime?.protocolTxPin ?? "-"}`,
    `MAVLink transport: ${j.runtime?.mavlinkTransport ?? "-"}`,
    `MAVLink RX/TX pins: ${j.runtime?.mavRxPin ?? "-"}/${j.runtime?.mavTxPin ?? "-"}`,
    "",
    `WiFi connected: ${bool(j.wifi?.connected)}`,
    `WiFi SSID: ${j.wifi?.ssid ?? "-"}`,
    `WiFi IP: ${j.wifi?.ip ?? "-"}`,
    `WiFi clients: ${j.wifi?.clients ?? "-"}`,
    "",
    `MAVLink connected: ${bool(j.mavlink?.connected)}`,
    `Target SYSID: ${j.mavlink?.sysid ?? "-"}`,
    `Target COMPID: ${j.mavlink?.compid ?? "-"}`,
    `Heartbeats: ${j.mavlink?.heartbeatCount ?? "-"}`,
    "",
    `Upload state: ${j.upload?.stateText ?? "-"}`,
    `Upload progress: ${j.upload?.progress ?? 0}%`,
    `Upload sent/total: ${j.upload?.sent ?? 0}/${j.upload?.total ?? 0}`,
    "",
    `Echo enabled: ${bool(j.settings?.echo)}`,
  ].join("\n");

  ui.protocol.textContent = [
    `RX state: ${j.protocol?.rxStateText ?? "-"}`,
    `RX state code: ${j.protocol?.rxState ?? "-"}`,
    `Last CRC OK: ${bool(j.protocol?.lastCrcOk)}`,
    `Last frame length: ${j.protocol?.lastFrameLen ?? 0}`,
    `Frame in progress: ${bool(j.protocol?.frameInProgress)}`,
    "",
    `RX bytes: ${j.logs?.totalRx ?? 0}`,
    `TX bytes: ${j.logs?.totalTx ?? 0}`,
    `RX hex len: ${j.logs?.rxHexLen ?? 0}`,
    `RX ascii len: ${j.logs?.rxAsciiLen ?? 0}`,
    `TX hex len: ${j.logs?.txHexLen ?? 0}`,
    `TX ascii len: ${j.logs?.txAsciiLen ?? 0}`,
  ].join("\n");

  echoEnabled = !!j.settings?.echo;
  ui.btnEcho.textContent = `Echo: ${echoEnabled ? "ON" : "OFF"}`;
}

async function refreshStatus() {
  try {
    const j = await request("/api/status");
    renderStatus(j);
  } catch (e) {
    ui.status.textContent = `Status error: ${e.message}`;
    $("switch-state").textContent = "Нет связи: состояние ключа неизвестно";
    $("sequence").textContent = "Нет связи с ESP. Последние данные устарели.";
    ui.btnUpload.disabled = true;
  }
}

async function refreshLogs() {
  try {
    const txt = await request("/api/logs");
    ui.logs.textContent = txt || "";
  } catch (e) {
    ui.logs.textContent = `Logs error: ${e.message}`;
  }
}

async function refreshMission() {
  try {
    const j = await request("/api/mission");
    ui.mission.textContent = JSON.stringify(j, null, 2);
  } catch (e) {
    ui.mission.textContent = `Mission error: ${e.message}`;
  }
}

async function loadRawMission() {
  try {
    const j = await request("/api/mission/raw");
    if (typeof j === "string") {
      ui.raw.textContent = j;
      return;
    }

    ui.raw.textContent = [
      `bytesLen: ${j.bytesLen ?? 0}`,
      `uploadState: ${j.uploadState ?? "-"}`,
      `uploadSent/Total: ${j.uploadSent ?? 0}/${j.uploadTotal ?? 0}`,
      "",
      "ASCII:",
      j.ascii ?? "",
      "",
      "BYTES HEX:",
      j.bytesHex ?? "",
    ].join("\n");
  } catch (e) {
    ui.raw.textContent = `Raw mission error: ${e.message}`;
  }
}

async function setEcho(value) {
  const body = new URLSearchParams();
  body.set("set", value ? "1" : "0");
  await request("/api/echo", { method: "POST", body });
  await refreshStatus();
}

function bindActions() {
  ui.btnUpload.addEventListener("click", async () => {
    try {
      await request("/api/upload/start", { method: "POST" });
      await refreshStatus();
    } catch (e) {
      alert(`Upload start failed: ${e.message}`);
    }
  });

  ui.btnStop.addEventListener("click", async () => {
    try {
      await request("/api/upload/stop", { method: "POST" });
      await refreshStatus();
    } catch (e) {
      alert(`Upload stop failed: ${e.message}`);
    }
  });

  ui.btnEcho.addEventListener("click", async () => {
    try {
      await setEcho(!echoEnabled);
    } catch (e) {
      alert(`Echo update failed: ${e.message}`);
    }
  });

  ui.btnRaw.addEventListener("click", loadRawMission);

  ui.btnMissionClear.addEventListener("click", async () => {
    try {
      await request("/api/mission/clear", { method: "POST" });
      ui.raw.textContent = "empty";
      await refreshMission();
      await refreshStatus();
    } catch (e) {
      alert(`Clear mission failed: ${e.message}`);
    }
  });

  ui.btnClear.addEventListener("click", async () => {
    try {
      await request("/api/logs/clear", { method: "POST" });
      await refreshLogs();
    } catch (e) {
      alert(`Clear logs failed: ${e.message}`);
    }
  });
}

async function init() {
  bindActions();
  await refreshStatus();
  await refreshMission();
  await refreshLogs();
  await loadRawMission();
  setInterval(refreshStatus, 1500);
  setInterval(refreshMission, 2000);
  setInterval(refreshLogs, 2500);
  setInterval(loadRawMission, 2000);
}

init();
