<template>
  <div class="not-found-view">
    <h2>404 - Page Not Found</h2>
    <p>The page you are looking for does not exist.</p>
    <RouterLink to="/">Go to Dashboard</RouterLink>
  </div>
</template>

<script setup lang="ts">
import { RouterLink } from 'vue-router';
</script>

<style scoped>
.not-found-view {
  padding: 40px 20px;
  text-align: center;
}
.not-found-view h2 {
  font-size: 2em;
  margin-bottom: 20px;
  color: var(--spindex-danger, #dc3545);
}
</style>
