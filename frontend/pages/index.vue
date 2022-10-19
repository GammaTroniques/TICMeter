<template>
    <div>
        <h1>Linky</h1>
        <img src="/imgs/linky.png" alt="" style="width: 200px;">

        <div id="live" class="windows" v-if="connected">
            <h2>Infomation en live <span class="mdi mdi-record" style="color: red;"></span></h2>
            <p>Index Heure Pleines: {{HCHP}} Wh</p>
            <p>Index Heure Creuses: {{HCHC}} Wh</p>
            <p>Index Base: {{BASE}} Wh</p>
            <p>Puissance Apparente: {{PAPP}} VA</p>
            <p>Intensité Instantanée: {{IINST}} A</p>
        </div>
        <div v-else>
            <h2>Connexion en cours...</h2>
        </div>
        <button @click="toto">Test</button>
        <button @click="refresh">Redresh</button>
        <div v-if="!pending">
            <h2>Historique</h2>
            {{toto2()}}
            <Bar :chart-data="chartData" chart-id="conso" :width="800" :chart-options="chartOptions" />
        </div>
    </div>
</template>
  

<script>
import { Bar } from 'vue-chartjs'
import { Chart as ChartJS, Title, Tooltip, Legend, BarElement, CategoryScale, LinearScale } from 'chart.js'

ChartJS.register(Title, Tooltip, Legend, BarElement, CategoryScale, LinearScale)

export default {
    name: 'BarChart',
    components: { Bar },
}
</script>

<script setup>

import { io } from "socket.io-client";

var HCHP = ref(0);
var HCHC = ref(0);
var BASE = ref(0);
var PAPP = ref(0);
var IINST = ref(0);


const { pending, data: posts, refresh } = useLazyAsyncData('data', () => $fetch('http://localhost:3001/get'))

var dataConso = [

]

function toto2() {
    for (let i = 0; i < posts.value.length; i++) {
        console.log(i);
        dataConso[i] = posts.value[i].BASE;
    }
}

watch(posts, (newPosts) => {
    toto2()
})

const socket = io("ws://localhost:3001");

var connected = ref(false);




var chartOptions = ref({
    plugins: {
        tooltip: {
            callbacks: {
                label: (context) => {
                    let label = context.dataset.label || '';

                    if (label) {
                        label += ': ';
                    }
                    if (context.parsed.y !== null) {
                        label += context.parsed.y + ' Wh';
                    }
                    return label;
                }
            }
        }
    }
});


var chartData = ref({
    labels: [
        "Janvier",
        "Février",
        "Mars",
        "Avril",
        "Mai",
        "Juin",
        "Juillet",
        "Août",
        "Septembre",
        "Octobre",
        "Novembre",
        "Décembre"
    ],
    datasets: [
        {
            label: 'Consommation',
            backgroundColor: '#004079',
            data: dataConso,
        }
    ]
})


onMounted(() => {
    socket.on("connect", () => {
        console.log("Connected");
        connected.value = socket.connected;

    });

    socket.on("disconnect", () => {
        connected.value = socket.connected;
    });

    socket.on("live", (data) => {
        console.log(data);
        if ("HCHP" in data) HCHP.value = data.HCHP;
        if ("HCHC" in data) HCHC.value = data.HCHC;
        if ("BASE" in data) BASE.value = data.BASE;
        if ("PAPP" in data) PAPP.value = data.PAPP;
        if ("IINST" in data) IINST.value = data.IINST;
    });

});

function toto() {
    socket.emit('live', {
        HCHC: 12,
    });
}


</script>
  
<style lang="scss">
.windows {
    border: 1px solid #000;
    border-radius: 20px;
}
</style>