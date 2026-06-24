FROM node:24-alpine
WORKDIR /app
COPY demo-backends/package.json ./
RUN npm install --omit=dev
COPY demo-backends/server.js ./
EXPOSE 3001
CMD ["npm", "start"]

