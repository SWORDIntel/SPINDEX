import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import path from 'path' // Import path module

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [vue()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'), // Alias @ to /src
    },
  },
  server: {
    port: 8080, // Optional: specify dev server port
    proxy: {
      // Optional: Proxy API requests to backend during development
      // Example:
      // '/api': {
      //   target: 'http://localhost:8000', // Your backend address
      //   changeOrigin: true,
      //   rewrite: (path) => path.replace(/^\/api/, '')
      // }
    }
  }
})
