<template>
  <div class="default-layout">
    <AppHeader />
    <div class="main-container">
      <AppSidebar />
      <div class="content-wrapper">
        <Breadcrumbs />
        <main class="content-area">
          <RouterView /> <!-- Main content will be injected here by the router -->
        </main>
      </div>
    </div>
    <AppFooter />
  </div>
</template>

<script setup lang="ts">
import AppHeader from '@/components/layout/AppHeader.vue'
import AppSidebar from '@/components/layout/AppSidebar.vue'
import AppFooter from '@/components/layout/AppFooter.vue'
import Breadcrumbs from '@/components/common/Breadcrumbs.vue' // Import Breadcrumbs
import { RouterView } from 'vue-router'

// No specific script logic for DefaultLayout itself for now,
// but it could manage overall layout state if needed in the future.
</script>

<style scoped>
.default-layout {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
  background-color: #f4f7f6; /* Or inherit from body */
}

.main-container {
  display: flex;
  flex-grow: 1;
  overflow: hidden; /* Prevent layout issues if sidebar or content overflows */
}

.content-wrapper {
  display: flex;
  flex-direction: column;
  flex-grow: 1;
  overflow-y: auto; /* Allow content area to scroll if it's too long */
}

.content-area {
  flex-grow: 1;
  padding: 20px;
  /* background-color: white; */ /* Optional: if content area needs distinct background */
}

/* Basic styling for AppHeader, AppSidebar, AppFooter will be in their own components.
   This layout primarily defines their arrangement.
   Example: ensure AppSidebar has a width and content-area flexes.
*/
</style>
