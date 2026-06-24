const state = {
  mode: "mixed",
  loop: null,
  totalSent: 0
};

const modeButtons = document.querySelectorAll("[data-mode]");
const countInput = document.querySelector("#countInput");
const pathInput = document.querySelector("#pathInput");
const sendButton = document.querySelector("#sendButton");
const loopButton = document.querySelector("#loopButton");
const serviceState = document.querySelector("#serviceState");
const sentCount = document.querySelector("#sentCount");
const lastRun = document.querySelector("#lastRun");
const statusMix = document.querySelector("#statusMix");
const resultLog = document.querySelector("#resultLog");

modeButtons.forEach((button) => {
  button.addEventListener("click", () => {
    state.mode = button.dataset.mode;
    modeButtons.forEach((item) => item.classList.toggle("selected", item === button));
    pathInput.disabled = state.mode !== "custom";
  });
});

sendButton.addEventListener("click", () => {
  sendTraffic().catch(showError);
});

loopButton.addEventListener("click", () => {
  if (state.loop) {
    clearInterval(state.loop);
    state.loop = null;
    loopButton.textContent = "Start loop";
    loopButton.classList.remove("active");
    serviceState.textContent = "Ready";
    return;
  }

  sendTraffic().catch(showError);
  state.loop = setInterval(() => sendTraffic().catch(showError), 1200);
  loopButton.textContent = "Stop loop";
  loopButton.classList.add("active");
});

pathInput.disabled = true;

async function sendTraffic() {
  serviceState.textContent = "Sending";
  const response = await fetch("/api/traffic", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      count: Number(countInput.value),
      mode: state.mode,
      path: pathInput.value
    })
  });
  const body = await response.json();
  if (!response.ok || !body.ok) {
    throw new Error(body.message || `Request failed with ${response.status}`);
  }

  state.totalSent += body.sent;
  sentCount.textContent = `${state.totalSent} sent`;
  lastRun.textContent = `${body.sent} requests in ${body.elapsed_ms} ms`;
  statusMix.textContent = summarizeStatuses(body.statuses);
  resultLog.textContent = JSON.stringify(body, null, 2);
  serviceState.textContent = state.loop ? "Looping" : "Ready";
}

function summarizeStatuses(statuses) {
  const counts = statuses.reduce((accumulator, status) => {
    accumulator[status] = (accumulator[status] || 0) + 1;
    return accumulator;
  }, {});
  return Object.entries(counts).map(([status, count]) => `${status}: ${count}`).join("  ");
}

function showError(error) {
  serviceState.textContent = "Error";
  resultLog.textContent = error.message;
}
