//***************************************************
// Page HTML et Javascript pour la Borne VE
// HP EVSE removed references to external google api script calls
//***************************************************
const char *BorneVEHtml = R"====(
  <!doctype html>
  <html><head><meta charset="UTF-8">
  <link rel="stylesheet" href="/commun.css">
  <style>
    input {font-size:18px;}
    .liste{display:flex;justify-content:center;text-align:left;} 
    #onglets2{display:block;}
    .Bparametres{border:inset 10px azure;}
    .BborneVE{border:inset 4px azure;}
  </style>
  <title>Borne VE - Configuration</title>
  </head>
  <body onload="Init();">
    <div id='lesOnglets'></div>

    <h4>Borne VE: ⚙️ Paramètres de fonctionnement</h4>
  
  <div style="text-align:left;">
  <form id="configForm">
    <table>
      <tr><td>Courant de charge minimum (6 A par défaut)</td><td><input type="number" step="0.1" id="I_min_c" name="I_min_c"></td></tr>
      <tr><td>Courant de charge maximum 10&lt;I_max&lt;32 (A)</td><td><input type="number" step="0.1" id="I_max" name="I_max"></td></tr>
      <tr><td>Tension réelle du réseau (V)</td><td><input type="number" step="1" id="U_reseau" name="U_reseau"></td></tr>
      <tr><td>Puissance minimum surplus 1200&lt;P_charge_min&lt;1500 (W)</td><td><input type="number" step="1" id="P_charge_min" name="P_charge_min"></td></tr>
      <tr><td>Puissance maximum soutirée -500&lt;P_charge_max&lt;-300 (W)</td><td><input type="number" step="1" id="P_charge_max" name="P_charge_max"></td></tr>
      <tr><td>Temps autorisé dépassement de la puissance (s)</td><td><input type="number" step="1" id="t_depassement" name="t_depassement"></td></tr>
      <tr><td>Temps d'attente entre le traitement (ms)</td><td><input type="number" step="1" id="t_delay_boucle" name="t_delay_boucle"></td></tr>
    </table>

      <div style="text-align:center;">
      <div class="radio-group" style="justify-content:center; margin-top:8px;">
      <label>Affichage sur écran :  </label>
        <label><input type="radio" name="affichageTypeEcran" value="0" checked> Pas d'écran</label>
        <label><input type="radio" name="affichageTypeEcran" value="1"> SSD1306</label>
        <label><input type="radio" name="affichageTypeEcran" value="2"> SH110x</label>
      </div>
    </div>

    <div style="text-align:left;">
      <label for="maxWhInput"><strong>Total énergie maximum à recharger (Wh), 0 = non actif :</strong></label>
      <input type="number" id="maxWhInput" name="maxWhInput" step="1" style="width:150px; margin-left:10px;">
    </div>

    <div style="text-align:center;">
    <div class="button-container">
      <br>
      <button type="button" class="save-btn" onclick="saveParams()">💾 Sauvegarder</button>
      <br><br> <!-- ✅ saut de ligne ajouté -->
   </div>
  </form>

    <script>
      var BordsInverse=[".Bparametres",".BborneVE"];
      function Init(){
        SetHautBas();
        LoadParaRouteur();
        LoadCouleurs();
        loadParams();
      }
      // Valeurs par défaut
      const defaultParams = {
        I_min_c: 6.0,
        I_max: 12.0,
        U_reseau: 235.0,
        P_charge_min: 1300.0,
        P_charge_max: -500.0,
        t_depassement: 15,
        t_delay_boucle: 1000,
        ecran_type: 0,
        maxWhInput:0
      };
      
      function loadParams() {
        fetch('/getParams')
        .then(r => r.json())
        .then(data => {
          if (!data || Object.keys(data).length === 0) data = defaultParams;

          document.getElementById('I_min_c').value = data.I_min_c || defaultParams.I_min_c;
          document.getElementById('I_max').value = data.I_max || defaultParams.I_max;
          document.getElementById('U_reseau').value = data.U_reseau || defaultParams.U_reseau;
          document.getElementById('P_charge_min').value = data.P_charge_min || defaultParams.P_charge_min;
          document.getElementById('P_charge_max').value = data.P_charge_max || defaultParams.P_charge_max;
          document.getElementById('t_depassement').value = data.t_depassement || defaultParams.t_depassement;
          document.getElementById('t_delay_boucle').value = data.t_delay_boucle || defaultParams.t_delay_boucle;
          document.getElementById('maxWhInput').value = data.maxWhInput || defaultParams.maxWhInput;

          const radio = document.querySelector(`input[name="affichageTypeEcran"][value="${data.ecran_type || 0}"]`);
          if (radio) radio.checked = true;
        })
        .catch(err => {
          console.error("Erreur de lecture paramètres :", err);
          for (const k in defaultParams) {
            const el = document.getElementById(k);
            if (el) el.value = defaultParams[k];
          }
        });
      }

    function saveParams() {
      const params = {
        I_min_c: parseFloat(document.getElementById('I_min_c').value),
        I_max: parseFloat(document.getElementById('I_max').value),
        U_reseau: parseFloat(document.getElementById('U_reseau').value),
        P_charge_min: parseFloat(document.getElementById('P_charge_min').value),
        P_charge_max: parseFloat(document.getElementById('P_charge_max').value),
        t_depassement: parseInt(document.getElementById('t_depassement').value),
        t_delay_boucle: parseInt(document.getElementById('t_delay_boucle').value),
        ecran_type: parseInt(document.querySelector('input[name="affichageTypeEcran"]:checked').value),
        maxWhInput: parseInt(document.getElementById('maxWhInput').value)
      };

      fetch('/saveParams', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
      })
      .then(r => r.text())
      .then(txt => {
        const statusEl = document.getElementById('status');
        if (statusEl) statusEl.innerHTML = txt;
      })
      .catch(err => {
        const statusEl = document.getElementById('status');
        if (statusEl) statusEl.innerHTML = "Erreur de sauvegarde !";
        console.error(err);
      });
    }  


    function updateMaxWh() {
      const maxWh = parseInt(document.getElementById('maxWhInput').value);
      if(isNaN(maxWh) || maxWh < 0) {
        alert('Valeur invalide');
        return;
      }
      fetch(`/setMaxWh?value=${maxWh}`)
        .then(response => response.text())
        .then(data => {
          console.log('Max Wh mis à jour:', data);
        });
    }

    function getTypeEcran() {
      // Récupère la valeur du bouton radio sélectionné
      ecran_type = document.querySelector('input[name="affichageTypeEcran"]:checked').value;
      console.log("Type d'écran sélectionné :", ecran_type);
      return ecran_type;
    }

    // Cette fonction est appelée au chargement de la page
    function setTypeEcran(ecran_type) {
      const radios = document.getElementsByName('affichageTypeEcran');
      for (let i = 0; i < radios.length; i++) {
        radios[i].checked = (radios[i].value == ecran_type);
      }
    }

    function AdaptationSource() {
      // Function intentionally left empty
    }

    function FinParaRouteur() {
      const heureEl = document.getElementById("Bheure");
      const wifiEl = document.getElementById("Bwifi");

      if (heureEl) heureEl.style.display = (Horloge > 1) ? "inline-block" : "none";
      if (wifiEl) wifiEl.style.display = (ESP32_Type < 10) ? "inline-block" : "none";

      LoadCouleurs();
    };
    </script>

    <div id='pied'></div>
    <br>
    <script src="/ParaRouteurJS"></script>
    <script src="/CommunCouleurJS"></script>
  </body></html>
)====";
