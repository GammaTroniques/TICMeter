FROM node:18-alpine

RUN mkdir -p /usr/linky
WORKDIR /usr/linky

COPY package*.json ./

RUN npm ci && npm cache clean --force

COPY . .

EXPOSE 3001

ENTRYPOINT ["node", "index.js"]