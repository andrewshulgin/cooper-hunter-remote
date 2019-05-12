FROM node:12-alpine

USER node
RUN mkdir /home/node/cooper-hunter-remote-api
WORKDIR /home/node/cooper-hunter-remote-api
COPY --chown=node:node . .

RUN npm install

CMD ["node", "server.js"]
