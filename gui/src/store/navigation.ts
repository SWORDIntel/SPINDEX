import { defineStore } from 'pinia'
import type { RouteLocationNormalized } from 'vue-router'

export interface BreadcrumbItem {
  text: string;
  to?: string; // Route path or name
  disabled?: boolean;
}

interface NavigationState {
  breadcrumbs: BreadcrumbItem[];
}

export const useNavigationStore = defineStore('navigation', {
  state: (): NavigationState => ({
    breadcrumbs: [],
  }),
  actions: {
    updateBreadcrumbs(route: RouteLocationNormalized) {
      const newBreadcrumbs: BreadcrumbItem[] = []
      // Attempt to build breadcrumbs from route meta
      // This is a simple example; a real app might have more complex logic
      // or rely on parent-child route relationships.

      if (route.meta?.breadcrumb) {
        if (typeof route.meta.breadcrumb === 'function') {
          newBreadcrumbs.push({ text: route.meta.breadcrumb(), to: route.path })
        } else {
          newBreadcrumbs.push({ text: route.meta.breadcrumb as string, to: route.path })
        }
      } else if (route.name) {
        // Fallback to route name if no breadcrumb meta
        newBreadcrumbs.push({ text: String(route.name), to: route.path })
      } else {
        newBreadcrumbs.push({text: 'Current Page', to: route.path})
      }

      // A more sophisticated approach would traverse matched routes:
      // route.matched.forEach(record => {
      //   if (record.meta.breadcrumb) {
      //     newBreadcrumbs.push({
      //       text: typeof record.meta.breadcrumb === 'function' ? record.meta.breadcrumb() : record.meta.breadcrumb,
      //       to: record.path
      //     });
      //   } else if (record.name) {
      //      newBreadcrumbs.push({ text: String(record.name), to: record.path });
      //   }
      // });

      // For this basic setup, we just take the current route's breadcrumb.
      // If it's the homepage, breadcrumb might be just "Dashboard" or empty.
      if (route.path === '/' && newBreadcrumbs.length > 0 && newBreadcrumbs[0].text.toLowerCase() === 'dashboard') {
        // Often, we don't show a clickable "Dashboard" breadcrumb when on the dashboard itself.
        // Or we might want a static "Home" breadcrumb always present.
        // This logic can be customized.
      }


      this.breadcrumbs = newBreadcrumbs
    },
    setBreadcrumbs(items: BreadcrumbItem[]) {
      this.breadcrumbs = items;
    }
  },
})
