FROM node:20-alpine
WORKDIR /app
COPY demo-traffic/package.json ./
COPY demo-traffic/ ./
EXPOSE 4010
CMD ["node", "server.js"]
