import express from "express";
import bodyParser from "body-parser";
import { createServer } from "http";

const PORT = 3000;

var TOKEN = "1234"; //token to authenticate the TICMeter

const app = express();
const server = createServer(app);

app.use(bodyParser.json()); //parse json body

app.get("/", (req, res) => {
  res.send("Hello World");
});

app.post("/post", async (req, res) => {
  //post request from esp32 to insert data in database
  const body = req.body;
  console.log(body);
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

  res.send("OK");
});

app.get("/config", async (req, res) => {
  console.log("TICMeter get config");
  if (!("token" in req.query)) {
    res.status(401).send("TOKEN is missing");
    return;
  }
  const token = req.query.token;
  if (token != TOKEN) {
    res.status(401).send("Invalid TOKEN"); //send error if token is invalid
    return;
  }

  const config = {
    refresh_rate: 60, // read data every X second. min 10s max 3600s
    store_before_send: 2, //send data every X read. min 0 max 10
  };

  res.send(config);
});

server.listen(PORT, () => {
  console.log(`Server is running on port ${PORT}`);
});
