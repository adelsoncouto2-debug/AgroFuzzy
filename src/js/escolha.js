const CONTROLLER_URL = "http://192.168.4.1/api/cultura";

const botoesCultura = document.querySelectorAll("[data-cultura]");
const statusRequisicao = document.querySelector(".request-status");

botoesCultura.forEach((botao) => {
  botao.addEventListener("click", async () => {
    const cultura = botao.dataset.cultura;
    statusRequisicao.textContent = "Enviando seleção...";
    botoesCultura.forEach((item) => {
      item.disabled = true;
    });

    try {
      const resposta = await fetch(CONTROLLER_URL, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ cultura }),
      });

      if (!resposta.ok) {
        throw new Error(`O controlador respondeu com HTTP ${resposta.status}.`);
      }

      statusRequisicao.textContent = "Cultura enviada ao controlador.";
    } catch (erro) {
      console.error("Não foi possível enviar a cultura ao controlador:", erro);
      statusRequisicao.textContent =
        "Não foi possível conectar ao controlador. Verifique a rede e tente novamente.";
    } finally {
      botoesCultura.forEach((item) => {
        item.disabled = false;
      });
    }
  });
});
