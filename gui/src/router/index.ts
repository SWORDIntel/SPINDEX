import { createRouter, createWebHistory, type RouteRecordRaw, type RouteMeta } from 'vue-router'
import { useNavigationStore } from '@/store/navigation'

// Extend RouteMeta interface for custom properties
declare module 'vue-router' {
  interface RouteMeta {
    requiresAuth?: boolean;
    layout?: string; // e.g., 'DefaultLayout', 'AuthLayout'
    breadcrumb?: string | (() => string); // string or function returning string
    roles?: string[]; // For RBAC
  }
}

const routes: Array<RouteRecordRaw> = [
  {
    path: '/',
    name: 'Home',
    component: () => import('@/views/HomeView.vue'), // Lazy loading
    meta: {
      requiresAuth: true,
      breadcrumb: 'Dashboard'
    }
  },
  {
    path: '/login',
    name: 'Login',
    component: () => import('@/views/LoginView.vue'), // Lazy loading
    meta: {
      requiresAuth: false,
      // layout: 'AuthLayout' // Example if using different layout for login
      breadcrumb: 'Login'
    }
  },
  {
    path: '/search',
    name: 'Search',
    component: () => import('@/views/SearchView.vue'), // Placeholder view
    meta: {
      requiresAuth: true,
      breadcrumb: 'Search'
    }
  },
  {
    path: '/indexes',
    name: 'IndexManagement',
    component: () => import('@/views/IndexManagementView.vue'), // Placeholder view
    meta: {
      requiresAuth: true,
      breadcrumb: 'Index Management'
    }
  },
  {
    path: '/history',
    name: 'QueryHistory',
    component: () => import('@/views/QueryHistoryView.vue'), // Placeholder view
    meta: {
      requiresAuth: true,
      breadcrumb: 'Query History'
    }
  },
  {
    path: '/settings',
    name: 'Settings',
    component: () => import('@/views/SettingsView.vue'), // Placeholder view
    meta: {
      requiresAuth: true,
      breadcrumb: 'Settings'
    }
  },
  // Example for admin section (to be used with sidebar structure)
  {
    path: '/admin/users',
    name: 'AdminUserManagement',
    component: () => import('@/views/admin/AdminUserManagementView.vue'), // Placeholder view
    meta: {
      requiresAuth: true,
      roles: ['admin'],
      breadcrumb: 'User Management'
    }
  },
  {
    path: '/admin/system',
    name: 'AdminSystemStatus',
    component: () => import('@/views/admin/AdminSystemStatusView.vue'), // Placeholder view
    meta: {
      requiresAuth: true,
      roles: ['admin'],
      breadcrumb: 'System Status'
    }
  },
  {
    path: '/admin/rbac',
    name: 'RBAC',
    component: () => import('@/views/admin/RBAC.vue'),
    meta: {
      requiresAuth: true,
      roles: ['admin'],
      breadcrumb: 'RBAC'
    }
  },
  {
    path: '/admin/e2e-encryption',
    name: 'E2EEncryption',
    component: () => import('@/views/admin/E2EEncryption.vue'),
    meta: {
      requiresAuth: true,
      roles: ['admin'],
      breadcrumb: 'E2E Encryption'
    }
  },
  // Catch-all 404
  {
    path: '/:pathMatch(.*)*',
    name: 'NotFound',
    component: () => import('@/views/NotFoundView.vue'),
    meta: {
      requiresAuth: false
    }
  }
]

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL), // Vite's base URL
  routes,
  scrollBehavior(to, from, savedPosition) {
    if (savedPosition) {
      return savedPosition
    } else {
      return { top: 0 }
    }
  }
})

// Placeholder for auth store - replace with actual Pinia store import
// import { useAuthStore } from '@/store/auth';

// Mock auth store for now, as Pinia store setup is a separate step
const useAuthStore = () => {
  // In a real scenario, this would come from Pinia state
  // For testing the guard logic, we can simulate it.
  // To test different scenarios, you might need to mock this function differently in tests
  // or set a global testing flag.
  // For now, let's assume the user is initially not authenticated for guard testing.
  return {
    isAuthenticated: false,
    // user: { roles: ['viewer'] } // Example user with roles
    user: null
  };
};


router.beforeEach((to, from, next) => {
  // It's important that the auth store is initialized before the router guard runs.
  // This usually happens in main.ts when Pinia is `app.use()`d.
  // If useAuthStore() is called here, Pinia must be active.
  // Ensure Pinia is installed and router is used AFTER Pinia in main.ts
  // const authStore = useAuthStore(); // This will be the actual store once created
  const authStore = useAuthStore(); // Using the mock for now
  const isAuthenticated = authStore.isAuthenticated;

  // Update breadcrumbs
  // This needs Pinia to be active, so useNavigationStore() can be called.
  // If stores are not yet fully set up in main.ts, this might error or not work.
  // For now, assuming it can be called.
  try {
    const navigationStore = useNavigationStore();
    navigationStore.updateBreadcrumbs(to);
  } catch (e) {
    console.error("Error updating breadcrumbs, Pinia store might not be available yet:", e);
  }

  const requiresAuth = to.meta.requiresAuth;
  const requiredRoles = to.meta.roles as string[] | undefined; // Type cast for safety

  if (requiresAuth && !isAuthenticated) {
    // If route requires auth and user is not authenticated, redirect to login
    next({ name: 'Login', query: { redirect: to.fullPath } });
  } else if (to.name === 'Login' && isAuthenticated) {
    // If user is authenticated and tries to access login page, redirect to home
    next({ name: 'Home' });
  } else if (requiresAuth && requiredRoles && requiredRoles.length > 0) {
    // Route requires auth and specific roles
    // const userRoles = authStore.user?.role ? [authStore.user.role] : []; // Adapt based on UserInfo structure
    // For now, assuming authStore.user.roles is an array if user exists
    const userRoles = (authStore.user as any)?.roles || []; // Placeholder for user roles
    const hasRequiredRole = requiredRoles.some(role => userRoles.includes(role));

    if (hasRequiredRole) {
      next(); // User has required role
    } else {
      // User does not have required role - redirect to an unauthorized page or home
      // For now, let's redirect to home with a conceptual warning.
      // In a real app, you might have a dedicated 'Unauthorized' view.
      console.warn(`User does not have required roles (${requiredRoles.join(', ')}) for route ${to.path}. Redirecting to Home.`);
      next({ name: 'Home' });
    }
  }
  else {
    // No specific auth/role requirements or requirements met
    next();
  }
});

export default router