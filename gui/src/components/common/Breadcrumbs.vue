<template>
  <nav aria-label="breadcrumb" class="breadcrumbs-container" v-if="breadcrumbs.length > 0">
    <ol class="breadcrumb-list">
      <!-- Optional: Always add a "Home" or "Dashboard" link at the beginning -->
      <!--
      <li class="breadcrumb-item">
        <RouterLink to="/">Home</RouterLink>
      </li>
      -->
      <li
        v-for="(crumb, index) in breadcrumbs"
        :key="index"
        class="breadcrumb-item"
        :class="{ active: index === breadcrumbs.length - 1 }"
      >
        <RouterLink v-if="crumb.to && index < breadcrumbs.length - 1" :to="crumb.to">
          {{ crumb.text }}
        </RouterLink>
        <span v-else :aria-current="index === breadcrumbs.length - 1 ? 'page' : undefined">
          {{ crumb.text }}
        </span>
      </li>
    </ol>
  </nav>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { RouterLink } from 'vue-router'
import { useNavigationStore, type BreadcrumbItem } from '@/store/navigation'

const navigationStore = useNavigationStore()
const breadcrumbs = computed<BreadcrumbItem[]>(() => navigationStore.breadcrumbs)
</script>

<style scoped>
.breadcrumbs-container {
  padding: 0.75rem 1rem;
  background-color: #e9ecef; /* Light grey background */
  border-bottom: 1px solid #dee2e6;
  font-size: 0.9em;
}

.breadcrumb-list {
  display: flex;
  flex-wrap: wrap;
  padding: 0;
  margin: 0;
  list-style: none;
}

.breadcrumb-item {
  display: flex;
  align-items: center;
}

.breadcrumb-item + .breadcrumb-item::before {
  content: '/'; /* Separator */
  padding: 0 0.5rem;
  color: #6c757d;
}

.breadcrumb-item a {
  color: var(--spindex-primary, #007bff);
  text-decoration: none;
}

.breadcrumb-item a:hover {
  text-decoration: underline;
}

.breadcrumb-item.active span {
  color: #495057; /* Darker color for active item */
  font-weight: bold;
}
</style>
