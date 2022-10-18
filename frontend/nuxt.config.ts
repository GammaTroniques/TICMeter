// https://v3.nuxtjs.org/api/configuration/nuxt.config
export default defineNuxtConfig({
    head: [
        ["link", { rel: "stylesheet", href: "https://cdn.jsdelivr.net/npm/@mdi/font@6.9.96/css/materialdesignicons.min.css" }],
    ],
    assets: "/<rootDir>/assets",
    css: [
        "assets/scss/style.scss",
    ],

    // modules: [
    //     'nuxt-socket-io',
    // ],
    // io: {
    //     // module options
    //     sockets: [{
    //         name: 'main',
    //         url: 'http://localhost:3000'
    //     }]
    // }
})
