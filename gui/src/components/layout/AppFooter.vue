<template>
  <footer class="app-footer">
    <p>&copy; {{ currentYear }} SPINDEX Project. All rights reserved.</p>
    <!-- Add other footer content like links if needed -->
    <!-- <p>
      <RouterLink to="/about">About</RouterLink> |
      <RouterLink to="/privacy">Privacy Policy</RouterLink>
    </p> -->
  </footer>
</template>

<script setup lang="ts">
// import { RouterLink } from 'vue-router' // Uncomment if using RouterLink in template
import { computed } from 'vue'

const currentYear = computed(() => new Date().getFullYear())
</script>

<style scoped>
.app-footer {
  padding: 15px 20px;
  text-align: center;
  background-color: #2c3e50; /* Same as header for consistency */
  color: #bdc3c7; /* Lighter text for footer */
  border-top: 1px solid #34495e;
  font-size: 0.9em;
}

.app-footer p {
  margin: 5px 0;
}

.app-footer a {
  color: #ecf0f1;
  text-decoration: none;
}

.app-footer a:hover {
  text-decoration: underline;
}
</style>
