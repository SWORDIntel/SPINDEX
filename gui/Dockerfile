# --- Builder Stage ---
FROM node:18-alpine AS builder

LABEL maintainer="JulesAI"
LABEL description="Builder stage for SPINDEX Vue.js Frontend"

WORKDIR /app

# Copy package files
COPY package.json package-lock.json* ./

# Install dependencies
RUN npm ci --silent

# Copy source files
COPY . .

# Build the application
RUN npm run build

# --- Runtime Stage ---
FROM nginx:alpine AS runtime

LABEL maintainer="JulesAI"
LABEL description="Runtime stage for SPINDEX Vue.js Frontend"

# Copy built assets from builder
COPY --from=builder /app/dist /usr/share/nginx/html

# Copy nginx configuration
COPY nginx.conf /etc/nginx/conf.d/default.conf

# Expose port 80
EXPOSE 80

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD wget --quiet --tries=1 --spider http://localhost/ || exit 1

# Start nginx
CMD ["nginx", "-g", "daemon off;"]
