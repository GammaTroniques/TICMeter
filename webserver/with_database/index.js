import express from 'express'
import { createServer } from "http";
import bodyParser from 'body-parser'
import cors from 'cors'
import { PrismaClient } from "@prisma/client"
import { Server } from "socket.io";
import sass from 'node-sass';


const prisma = new PrismaClient(); //create prisma client to connect to database

var TOKEN = null; //token to authenticate web and esp clients


//get token from database
await prisma.config.findMany().then((result) => {
    var toSend = {};
    result.forEach((element) => {
        toSend[element.prop] = element.value;
    });
    TOKEN = toSend.TOKEN;
}).catch((error) => {
    console.log(error);
});

const app = express() //create express app
const httpServer = createServer(app); //create http server

const io = new Server(httpServer, { //set up socket.io and bind it to our http server.
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});

io.on("connection", (socket) => { //connection event
    //console.log("a user connected: " + socket.id);
    socket.on("disconnect", () => {
        //console.log("user disconnected");
    });
    socket.on("get_data", async(message) => { //get_data event from web client to get data from database for chart
        if ("start" in message && "end" in message && message.start !== undefined && message.end !== undefined) { //if start and end date are defined, get data between start and end date
            //console.log("get_data: " + message.start + " " + message.end);
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
                var errormsg = "Error: " + error;
                socket.emit("error", errormsg);
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
    socket.on("get_live", async(message) => { //get_live event from web client to get live data from database
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

    socket.on("get_config", async(message) => { //get_config event from web client to get config from database
        const result = await prisma.config.findMany({});
        var toSend = {};
        result.forEach((element) => { //convert config to object
            toSend[element.prop] = element.value;
        });
        socket.emit("config_data", toSend);
    });

    socket.on("set_config", async(config) => { //set_config event from web client to set config in database
        var strError = "";
        for (const [key, value] of Object.entries(config)) { //loop through config object and update database
            await prisma.config.update({
                where: {
                    prop: key
                },
                data: {
                    value: value.toString()
                }
            }).catch((error) => {
                console.log(error);
                strError += error + "\n";
            });
        }
        if (strError != "") { //if there is an error, send it to web client
            socket.emit("error", strError);
        } else {
            const result = await prisma.config.findMany({}); //if there is no error, send new config to web client
            var toSend = {};
            result.forEach((element) => {
                toSend[element.prop] = element.value;
            });
            socket.emit("config_data", toSend);
        }
    });
});


function bigIntToString(object) { //convert bigint (from database) to string (JSON.stringify can't handle bigint)
    for (let key in object) {
        if (typeof object[key] === 'bigint') {
            object[key] = object[key].toString();
        }
    }
    return object;
}

function arrayBigIntToString(array) { //convert array of bigint (from database) to array of string (JSON.stringify can't handle bigint)
    for (let i = 0; i < array.length; i++) {
        array[i] = bigIntToString(array[i]);
    }
    return array;
}

app.use(cors()); //allow cors for all origins
app.use(bodyParser.json()); //parse json body
app.use(bodyParser.urlencoded({ extended: true })); //parse urlencoded body

app.get('/index.css', (req, res) => {
    sass.render({ // compile scss to css
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

app.use('/', express.static('public')); //serve static files from public folder

app.get("/post", (req, res) => { //only post requests are allowed
    res.send("Only post requests are allowed");
});

app.post("/post", async(req, res) => { //post request from esp32 to insert data in database
    const body = req.body;

    //--------------------check if the request is valid--------------------
    if (!("TOKEN" in body)) {
        console.log("Post Config: TOKEN is missing");
        res.status(400).send("TOKEN is missing");
        return;
    }

    if (body.TOKEN != TOKEN) {
        console.log("Post Config: invalid TOKEN");
        res.status(400).send("invalid TOKEN");
        return;
    }

    if (!("data" in body)) {
        console.log("Post Config: data is missing");
        res.status(400).send("data is missing");
        return;
    }
    //---------------------------------------------------------------------

    var dataToInsert = []; //array of data to insert in database
    body.data.forEach((data) => {
        let HCHC = null;
        let HCHP = null;
        let BASE = null;
        let PAPP = null;
        let IINST = null;
        let DATE = null;

        // -------------------check if data is valid-------------------
        if ("HCHC" in data && !isNaN(data.HCHC)) {
            HCHC = data.HCHC;
        }
        if ("HCHP" in data && !isNaN(data.HCHP)) {
            HCHP = data.HCHP;
        }
        if ("BASE" in data && !isNaN(data.BASE)) {
            BASE = data.BASE;
        }
        if ("PAPP" in data && !isNaN(data.PAPP)) {
            PAPP = data.PAPP;
        }
        if ("IINST" in data && !isNaN(data.IINST)) {
            IINST = data.IINST;
        }
        if ("DATE" in data) {
            DATE = data.DATE;
        }
        // -----------------------------------------------------------
        dataToInsert.push({ //add data to array
            date: new Date(DATE * 1000),
            HCHC: HCHC,
            HCHP: HCHP,
            BASE: BASE,
            PAPP: PAPP,
            IINST: IINST,
        });
    });
    console.log(`Inserting ${dataToInsert.length} data in database`); //insert data in database

    const result = await prisma.conso.createMany({ //insert data in database
        data: dataToInsert,
        skipDuplicates: true,
    });

    const lastData = body.data[body.data.length - 1]; //get last data
    lastData.DATE = new Date(lastData.DATE * 1000); //convert timestamp to date
    const live = await prisma.live.update({ //update live data
        where: {
            id: 0,
        },
        data: {
            date: lastData.DATE,
            BASE: lastData.BASE,
            HCHC: lastData.HCHC,
            HCHP: lastData.HCHP,
            ADCO: lastData.ADCO.toString(),
            OPTARIF: lastData.OPTARIF,
            ISOUSC: lastData.ISOUSC,
            PTEC: lastData.PTEC,
            IINST: lastData.IINST,
            PAPP: lastData.PAPP,
            HHPHC: lastData.HHPHC,
            MOTDETAT: lastData.MOTDETAT,
            VCONDO: body.VCONDO,
        },
    });
    res.send("OK");
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
});

app.get("/config", async(req, res) => { //get config from database (only for esp32)
    console.log("Esp32 get config");
    if (!("token" in req.query)) {
        res.status(401).send("TOKEN is missing");
        return;
    }
    const token = req.query.token;

    if (token == TOKEN) { //check if token is valid
        const result = await prisma.config.findMany() //get config from database
            .then((result) => {
                var toSend = {};
                result.forEach((element) => { //convert array to object
                    toSend[element.prop] = element.value;
                });
                res.send(toSend); //send config to esp32
            }).catch((error) => {
                console.log(error);
                res.status(500).send(error);
            });
    } else {
        res.status(401).send("TOKEN is invalid"); //send error if token is invalid
    }
});

app.get('*', function(req, res) { //other routes
    //res.redirect('/')
    //404 error
    res.send('404 Not Found', 404);
});

httpServer.listen(3001, () => {
    console.log('listening on *:3001');
});