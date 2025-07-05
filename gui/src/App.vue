<script setup lang="ts">
// DefaultLayout will handle RouterView
import DefaultLayout from '@/layouts/DefaultLayout.vue'

// If you plan to have routes that DON'T use DefaultLayout (e.g., a login page with AuthLayout),
// you'd typically compute the layout component here based on route meta fields.
// For now, we'll assume DefaultLayout is global for all routed views.
// Example for dynamic layout:
// import { useRoute } from 'vue-router'
// import { computed } from 'vue'
// const route = useRoute()
// const layout = computed(() => {
//   return route.meta.layout || DefaultLayout // Default to DefaultLayout
// })
</script>

<template>
  <!-- <component :is="layout"> -->
    <!-- <RouterView /> This would be inside the dynamic layout if RouterView isn't in DefaultLayout -->
  <!-- </component> -->
  <DefaultLayout />
</template>

<style>
/* Global styles are now primarily in assets/main.css */
/* It's good practice to keep App.vue's styles minimal or scoped if specific to App.vue itself */
#app {
  font-weight: normal; /* Retain this if it's a specific app-wide override */
}
</style>
