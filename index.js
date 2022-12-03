import express from 'express'
import { createServer } from "http";
import bodyParser from 'body-parser'
import cors from 'cors'
import { PrismaClient } from "@prisma/client"
import { Server } from "socket.io";
import sass from 'node-sass';



const prisma = new PrismaClient();

const TOKEN = "abc";

const app = express()
const httpServer = createServer(app);
const io = new Server(httpServer, {
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});

io.on("connection", (socket) => {
    console.log("a user connected: " + socket.id);
    socket.on("disconnect", () => {
        console.log("user disconnected");
    });
    socket.on("get_data", (message) => {
        if ("start" in message && "end" in message && message.start !== undefined && message.end !== undefined) {
            console.log("get_data: " + message.start + " " + message.end);
            prisma.conso.findMany({
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
                console.log(data);
                socket.emit("data", arrayBigIntToString(data));
            });
        } else {
            console.log(message);
            if ("count" in message) {
                prisma.conso.findMany({
                    take: message.count,
                }).then((data) => {
                    socket.emit("data", arrayBigIntToString(data));
                });
            }

        }

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



app.use(cors())
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

app.post("/post", async(req, res) => {
    const body = req.body;

    var HCHC = null;
    var HCHP = null;
    var BASE = null;
    var PAPP = null;
    var IINST = null;

    if (!("TOKEN" in body)) {
        res.send("TOKEN is missing");
        return;
    }

    if (body.TOKEN != TOKEN) {
        res.send("TOKEN is invalid");
        return;
    }

    if (!("DEST" in body)) {
        res.send("DEST (destination) is missing");
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

    if (body.DEST == "db") {
        const result = await prisma.conso.create({
            data: {
                HCHC: HCHC,
                HCHP: HCHP,
                BASE: BASE,
                PAPP: PAPP,
                IINST: IINST,
            },
        });
        console.log("Saved in database:");
        console.log(result);
        prisma.$disconnect();
        var toSend = bigIntToString(result);
        prisma.conso.findMany({
            take: 10,
        }).then((data) => {
            io.emit("data", arrayBigIntToString(data));
        });
        res.send("OK");
    } else if (body.DEST == "live") {
        console.log("Sent to live:");
        io.emit("live", {
            HCHC: HCHC,
            HCHP: HCHP,
            BASE: BASE,
            PAPP: PAPP,
            IINST: IINST,
        });
        res.send("OK");
    }

});


app.get("/get", async(req, res) => {
    const result = await prisma.conso.findMany();
    var toSend = arrayBigIntToString(result);
    console.log(toSend);
    res.send(toSend);
});

app.get("/config", async(req, res) => {
    const result = await prisma.config.findMany();
    res.send(result);
});


app.get('*', function(req, res) {
    //res.redirect('/')
    //404 error
    res.send('404 Not Found', 404);
});

httpServer.listen(3001, () => {
    console.log('Example app listening on port 3001!')
});