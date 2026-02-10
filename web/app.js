let ws = null;

const el = (id) => document.getElementById(id);

const joinCard = el("joinCard");
const chatCard = el("chatCard");

const nameInput = el("nameInput");
const joinBtn = el("joinBtn");
const joinHint = el("joinHint");

const meName = el("meName");
const messages = el("messages");
const msgInput = el("msgInput");
const sendBtn = el("sendBtn");

const dot = el("dot");
const statusText = el("statusText");

function setStatus(state) {
  // state: "ok" | "bad" | "idle"
  dot.classList.remove("ok", "bad");
  if (state === "ok") dot.classList.add("ok");
  if (state === "bad") dot.classList.add("bad");
}

function setStatusText(t) {
  statusText.textContent = t;
}

function addMessage(kind, from, text) {
  const row = document.createElement("div");
  row.className = "msg" + (kind === "system" ? " system" : "");

  const meta = document.createElement("div");
  meta.className = "meta";
  meta.textContent = kind === "system" ? "system" : from;

  const bubble = document.createElement("div");
  bubble.className = "bubble";
  bubble.textContent = text;

  row.appendChild(meta);
  row.appendChild(bubble);
  messages.appendChild(row);
  messages.scrollTop = messages.scrollHeight;
}

function wsUrl() {
  // later when we put WS behind nginx (/ws), this will change to wss://.../ws
  return `ws://${location.hostname}:9002`;
}

function connect() {
  return new Promise((resolve, reject) => {
    const url = wsUrl();
    setStatus("idle");
    setStatusText("connecting…");

    ws = new WebSocket(url);

    ws.onopen = () => {
      setStatus("ok");
      setStatusText("connected");
      resolve();
    };

    ws.onerror = (e) => {
      setStatus("bad");
      setStatusText("error");
      reject(e);
    };

    ws.onclose = () => {
      setStatus("bad");
      setStatusText("disconnected");
      sendBtn.disabled = true;
    };

    ws.onmessage = (e) => {
      let obj = null;
      try {
        obj = JSON.parse(e.data);
      } catch {
        addMessage("system", "system", `non-json from server: ${e.data}`);
        return;
      }

      if (obj.type === "system") {
        addMessage("system", "system", obj.text || "");
      } else if (obj.type === "msg") {
        addMessage("msg", obj.from || "?", obj.text || "");
      } else if (obj.type === "debug_join" || obj.type === "debug_msg") {
        // keep debug visible but subtle
        addMessage("system", "debug", `${obj.type}: ${JSON.stringify(obj)}`);
      } else if (obj.type === "error") {
        addMessage("system", "error", obj.text || "unknown error");
      } else {
        addMessage("system", "system", `unknown payload: ${JSON.stringify(obj)}`);
      }
    };
  });
}

async function joinLobby() {
  const name = nameInput.value.trim();
  if (!name) {
    joinHint.textContent = "Please enter a name.";
    return;
  }
  joinHint.textContent = "";

  joinBtn.disabled = true;
  nameInput.disabled = true;

  try {
    await connect();

    // send join
    ws.send(JSON.stringify({ type: "join", user: name, room: "lobby" }));

    // show chat UI
    meName.textContent = name;
    joinCard.style.display = "none";
    chatCard.style.display = "block";
    sendBtn.disabled = false;

    addMessage("system", "system", `You joined the lobby as ${name}.`);
    msgInput.focus();
  } catch (e) {
    joinBtn.disabled = false;
    nameInput.disabled = false;
    joinHint.textContent = "Could not connect. Check server and port 9002.";
  }
}

function sendMessage() {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    addMessage("system", "system", "not connected");
    return;
  }
  const text = msgInput.value.trim();
  if (!text) return;
  ws.send(JSON.stringify({ type: "msg", text }));
  msgInput.value = "";
}

joinBtn.addEventListener("click", joinLobby);
nameInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") joinLobby();
});

sendBtn.addEventListener("click", sendMessage);
msgInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") sendMessage();
});

// nice default
setStatus("bad");
setStatusText("disconnected");