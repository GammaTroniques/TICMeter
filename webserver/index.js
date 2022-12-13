import express from 'express'
import { createServer } from "http";
import bodyParser from 'body-parser'
import cors from 'cors'
import { PrismaClient } from "@prisma/client"
import { Server } from "socket.io";
import sass from 'node-sass';


const prisma = new PrismaClient();

var TOKEN = null;
await prisma.config.findMany({}).then((result) => {
    var toSend = {};
    result.forEach((element) => {
        toSend[element.prop] = element.value;
    });
    TOKEN = toSend.TOKEN;
}).catch((error) => {
    console.log(error);
});

const app = express()
const httpServer = createServer(app);

const io = new Server(httpServer, { //set up socket.io and bind it to our http server.
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});

io.on("connection", (socket) => { //connection event
    console.log("a user connected: " + socket.id);
    socket.on("disconnect", () => {
        console.log("user disconnected");
    });
    socket.on("get_data", async(message) => { //get_data event from web client to get data from database for chart
        if ("start" in message && "end" in message && message.start !== undefined && message.end !== undefined) { //if start and end date are defined, get data between start and end date
            console.log("get_data: " + message.start + " " + message.end);
            await prisma.conso.findMany({
                where: {
                    date: {
                        gte: message.start,
                        lte: message.end
                    }
                },
                orderBy: {
                    date: "asc"
                }
            }).then((data) => {
                socket.emit("chart_data", arrayBigIntToString(data));
            }).catch((error) => {
                console.log(JSON.stringify(error));
                socket.emit("error", error);
            });

        } else { //if start and end date are not defined, get last 1000 data
            console.log(message);
            if ("count" in message) {
                try {
                    await prisma.conso.findMany({
                        take: message.count,
                    }).then((data) => {
                        socket.emit("chart_data", arrayBigIntToString(data));
                    });
                } catch (error) {
                    console.log(error);
                }
            }

        }

    });
    socket.on("get_live", async(message) => {
        try {
            await prisma.live.findUnique({
                where: {
                    id: 0
                }
            }).then((data) => {
                socket.emit("live_data", bigIntToString(data));
            });
        } catch (error) {
            console.log(error);
        }
    });

    socket.on("get_config", async(message) => {
        const result = await prisma.config.findMany({});
        var toSend = {};
        result.forEach((element) => {
            toSend[element.prop] = element.value;
        });
        socket.emit("config_data", toSend);
    });

});


function bigIntToString(object) {
    for (let key in object) {
        if (typeof object[key] === 'bigint') {
            object[key] = object[key].toString();
        }
    }
    return object;
}

function arrayBigIntToString(array) {
    for (let i = 0; i < array.length; i++) {
        array[i] = bigIntToString(array[i]);
    }
    return array;
}



app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));


app.get('/index.css', (req, res) => {
    sass.render({
        file: './public/index.scss',
        // outputStyle: 'compressed'
    }, (err, result) => {
        if (err) {
            console.log(err);
            res.status(500).send(err);
        } else {
            res.set('Content-Type', 'text/css');
            res.send(result.css);
        }
    });
});


app.use('/', express.static('public'));

app.get("/post", (req, res) => {
    res.send("Only post requests are allowed");
});


var lastSendTime = 0;

app.post("/post", async(req, res) => {
    const body = req.body;
    console.log(body);
    var HCHC = null;
    var HCHP = null;
    var BASE = null;
    var PAPP = null;
    var IINST = null;
    var DATE = null;

    if (!("TOKEN" in body)) {
        console.log("TOKEN is missing");
        res.send("TOKEN is missing");
        return;
    }

    if (body.TOKEN != TOKEN) {
        console.log("TOKEN is invalid");
        res.send("TOKEN is invalid");
        return;
    }

    if ("HCHC" in body && !isNaN(body.HCHC)) {
        HCHC = body.HCHC;
    }
    if ("HCHP" in body && !isNaN(body.HCHP)) {
        HCHP = body.HCHP;
    }
    if ("BASE" in body && !isNaN(body.BASE)) {
        BASE = body.BASE;
    }
    if ("PAPP" in body && !isNaN(body.PAPP)) {
        PAPP = body.PAPP;
    }
    if ("IINST" in body && !isNaN(body.IINST)) {
        IINST = body.IINST;
    }

    if ("DATE" in body) {
        DATE = body.DATE;
    }
    try {
        const result = await prisma.conso.create({
            data: {
                date: new Date(DATE * 1000),
                HCHC: HCHC,
                HCHP: HCHP,
                BASE: BASE,
                PAPP: PAPP,
                IINST: IINST,
            },
        });

        console.log("Saved in database:");
        console.log(result);
        res.send("OK");


        // if time is more than the last send time
        if (new Date(DATE * 1000) - lastSendTime > 1) {
            console.log("Sending live data");
            lastSendTime = new Date(DATE * 1000);
            const live = await prisma.live.update({
                where: {
                    id: 0,
                },
                data: {
                    date: new Date(DATE * 1000),
                    HCHC: HCHC,
                    HCHP: HCHP,
                    BASE: BASE,
                    ADCO: body.ADCO.toString(),
                    OPTARIF: body.OPTARIF,
                    ISOUSC: body.ISOUSC,
                    PTEC: body.PTEC,
                    IINST: IINST,
                    IMAX: body.IMAX,
                    PAPP: PAPP,
                    HHPHC: body.HHPHC,
                    MOTDETAT: body.MOTDETAT,
                    VCONDO: body.VCONDO,
                },
            });
        }
    } catch (error) {
        console.log(error);
        res.status(500).send(error);
    }
});


app.get("/get", async(req, res) => {
    console.log(req.query);
    if ("live" in req.query) {
        try {
            const result = await prisma.live.findUnique({
                where: {
                    id: 0
                }
            });
            res.send(result);
        } catch (error) {
            console.log(error);
            res.status(500).send(error);
        }
    }

    // try {
    //     const result = await prisma.conso.findMany({
    //         take: 100,
    //     });
    //     var toSend = arrayBigIntToString(result);
    //     res.send(toSend);
    // } catch (error) {
    //     console.log(error);
    //     res.status(500).send(error);
    // }
});

app.get("/config", async(req, res) => {

    console.log(req.query);
    if (!("token" in req.query)) {
        res.status(401).send("TOKEN is missing");
        return;
    }

    const token = req.query.token;

    if (token == TOKEN) {
        const result = await prisma.config.findMany({})
            .then((result) => {
                var toSend = {};
                result.forEach((element) => {
                    toSend[element.prop] = element.value;
                });
                res.send(toSend);
            }).catch((error) => {
                console.log(error);
                res.status(500).send(error);
            });
    } else {
        res.status(401).send("TOKEN is invalid");
    }
    console.log("esp get config");
});


app.get('*', function(req, res) {
    //res.redirect('/')
    //404 error
    res.send('404 Not Found', 404);
});

httpServer.listen(3001, () => {
    console.log('Example app listening on port 3001!')
});