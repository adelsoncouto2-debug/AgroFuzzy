const DEFAULT_CONTROLLER = "http://192.168.4.1";
const controllerInput = document.querySelector("#controller-url");
const settingsDialog = document.querySelector("#settings-dialog");
const statusPanel = document.querySelector("#status-panel");
const statusCulture = document.querySelector("#status-culture");
const statusMessage = document.querySelector("#status-message");
const statusDose = document.querySelector("#status-dose");
const statusVolume = document.querySelector("#status-volume");
const statusPump = document.querySelector("#status-pump");

function controllerBase() {
  return (localStorage.getItem("agrofuzzy_controller") || DEFAULT_CONTROLLER).replace(/\/$/, "");
}

function setControllerBase(value) {
  let url;
  try {
    url = new URL(value);
  } catch {
    throw new Error("Informe um endereço válido.");
  }
  if (!["http:", "https:"].includes(url.protocol)) {
    throw new Error("Use um endereço HTTP ou HTTPS.");
  }
  localStorage.setItem("agrofuzzy_controller", url.origin);
}

async function atualizarStatus() {
  const resposta = await fetch(`${controllerBase()}/api/status`);
  if (!resposta.ok) throw new Error(`O controlador respondeu com HTTP ${resposta.status}.`);
  const dados = await resposta.json();

  statusPanel.hidden = false;
  statusCulture.textContent = dados.cultura;
  statusMessage.textContent = dados.motivo || "Estado atual";
  statusDose.textContent = dados.dose_pct ?? "--";
  statusVolume.textContent = dados.volume_ml ?? "--";
  statusPump.textContent = dados.bomba_ligada ? "Bomba ligada" : "Bomba desligada";
}

document.querySelector("#back-button").addEventListener("click", () => history.back());
document.querySelector("#settings-button").addEventListener("click", () => settingsDialog.showModal());
controllerInput.value = controllerBase();

document.querySelector("#save-controller").addEventListener("click", () => {
  try {
    setControllerBase(controllerInput.value.trim());
    settingsDialog.close();
    statusMessage.textContent = "Controlador salvo.";
  } catch (erro) {
    alert(erro.message);
  }
});

document.querySelectorAll("[data-cultura]").forEach((botao) => {
  botao.addEventListener("click", async () => {
    const cultura = botao.dataset.cultura;
    document.querySelectorAll("[data-cultura]").forEach((item) => (item.disabled = true));
    statusPanel.hidden = false;
    statusCulture.textContent = cultura;
    statusMessage.textContent = "Enviando seleção...";

    try {
      const resposta = await fetch(`${controllerBase()}/api/cultura`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ cultura })
      });
      if (!resposta.ok) throw new Error(`O controlador respondeu com HTTP ${resposta.status}.`);

      const dados = await resposta.json();
      statusMessage.textContent = "Cultura enviada.";
      statusDose.textContent = dados.dose_pct ?? "--";
      statusVolume.textContent = dados.volume_ml ?? "--";
      statusPump.textContent = dados.bomba_ligada ? "Bomba ligada" : "Bomba desligada";
      await atualizarStatus();
    } catch (erro) {
      console.error(erro);
      statusMessage.textContent = "Não foi possível conectar. Verifique a rede Wi-Fi.";
    } finally {
      document.querySelectorAll("[data-cultura]").forEach((item) => (item.disabled = false));
    }
  });
});

atualizarStatus().catch(() => {
  statusPanel.hidden = false;
  statusMessage.textContent = "Controlador não conectado.";
});
