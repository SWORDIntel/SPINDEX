# SPINDEX Dark Theme

## Overview

SPINDEX uses a **professional dark theme** as the default UI experience. The theme is designed for extended usage with reduced eye strain, excellent contrast ratios, and modern aesthetics.

---

## Theme Colors

### Background Colors
- **Primary Background:** `#0f1419` - Main application background
- **Secondary Background:** `#1a1f2e` - Cards, panels, sidebar
- **Tertiary Background:** `#252b3b` - Input fields, code blocks
- **Hover State:** `#2d3548` - Interactive element hover
- **Active State:** `#3a4152` - Active/pressed elements

### Text Colors
- **Primary Text:** `#e4e6eb` - Headings, important text
- **Secondary Text:** `#b0b3b8` - Body text, labels
- **Muted Text:** `#8a8d91` - Placeholders, hints
- **Inverse Text:** `#0f1419` - Text on light backgrounds

### Accent Colors
- **Primary Accent:** `#4c9aff` - Buttons, links, focus states
- **Secondary Accent:** `#00d4aa` - Success states, highlights
- **Warning:** `#ffab00` - Warnings, attention needed
- **Error:** `#ff5630` - Errors, destructive actions
- **Success:** `#36b37e` - Success messages, confirmations

### Branding Gradient
The SPINDEX logo and primary buttons use a gradient:
```css
background: linear-gradient(135deg, #4c9aff, #00d4aa);
```

---

## Implementation Files

### 1. Main Stylesheet
**Location:** `gui/style.css` (700 lines)

Comprehensive dark theme with:
- CSS custom properties (CSS variables)
- Component styles (buttons, forms, tables, cards)
- Layout system (header, sidebar, main content, footer)
- Responsive design (mobile, tablet, desktop)
- Animations and transitions
- Utility classes

### 2. Vue Global Styles
**Location:** `gui/src/assets/main.css` (337 lines)

Vue-specific dark theme with:
- CSS variables for Vue components
- Typography system
- Form element styling
- Button variants
- Card components
- Responsive utilities
- Scrollbar styling

### 3. Configuration
**Location:** `gui/.env.example`

Environment variable configuration:
```bash
VITE_THEME=dark
VITE_PRIMARY_COLOR=#4c9aff
VITE_SECONDARY_COLOR=#00d4aa
VITE_BG_PRIMARY=#0f1419
VITE_BG_SECONDARY=#1a1f2e
```

### 4. HTML Meta Tags
**Location:** `gui/index.html`

Proper meta tags for dark theme:
```html
<meta name="theme-color" content="#0f1419">
<meta name="color-scheme" content="dark">
```

---

## Features

### 1. Ergonomic Design
- **Reduced Blue Light:** Darker backgrounds reduce eye strain
- **High Contrast:** WCAG AA compliant contrast ratios
- **Smooth Transitions:** 0.15s-0.3s transitions for smooth interactions
- **Anti-Aliased Fonts:** `-webkit-font-smoothing: antialiased`

### 2. Modern Aesthetics
- **Gradient Accents:** Beautiful gradients for CTAs and branding
- **Subtle Shadows:** Depth and hierarchy with box-shadows
- **Rounded Corners:** 4px-12px border-radius for modern look
- **Hover Effects:** Micro-interactions for better UX

### 3. Responsive Design
- **Mobile First:** Optimized for mobile devices
- **Tablet Support:** Adaptive layouts for tablets
- **Desktop Optimized:** Full-featured desktop experience
- **Collapsible Sidebar:** Space-efficient navigation

### 4. Component System
- **Buttons:** Primary, secondary, danger, success variants
- **Forms:** Input fields, selects, textareas with focus states
- **Cards:** Content containers with hover effects
- **Tables:** Striped rows with hover highlights
- **Badges:** Status indicators (success, warning, error, info)
- **Alerts:** Notification components with severity levels
- **Spinners:** Loading indicators
- **Progress Bars:** Task progress visualization

---

## CSS Custom Properties

All theme values use CSS custom properties (CSS variables) for:
- **Easy Customization:** Change colors in one place
- **Runtime Updates:** Can be changed dynamically
- **Consistency:** Ensures consistent values across components
- **Maintainability:** Single source of truth

Example:
```css
:root {
  --bg-primary: #0f1419;
  --text-primary: #e4e6eb;
  --accent-primary: #4c9aff;
}

body {
  background-color: var(--bg-primary);
  color: var(--text-primary);
}

button {
  background-color: var(--accent-primary);
}
```

---

## Usage Examples

### Buttons
```html
<!-- Primary button (gradient) -->
<button class="btn">Upload File</button>

<!-- Secondary button -->
<button class="btn btn-secondary">Cancel</button>

<!-- Danger button -->
<button class="btn btn-danger">Delete</button>

<!-- Success button -->
<button class="btn btn-success">Confirm</button>
```

### Cards
```html
<div class="card">
  <div class="card-header">Index Statistics</div>
  <div class="card-body">
    <p>Total indexes: 42</p>
    <p>Active jobs: 7</p>
  </div>
</div>
```

### Badges
```html
<span class="badge badge-success">Active</span>
<span class="badge badge-warning">Pending</span>
<span class="badge badge-error">Failed</span>
<span class="badge badge-info">Processing</span>
```

### Alerts
```html
<div class="alert alert-success">
  File uploaded successfully!
</div>

<div class="alert alert-error">
  Upload failed. Please try again.
</div>
```

### Utility Classes
```html
<!-- Text colors -->
<p class="text-primary">Primary text</p>
<p class="text-secondary">Secondary text</p>
<p class="text-muted">Muted text</p>

<!-- Spacing -->
<div class="mt-lg">Margin top large</div>
<div class="p-md">Padding medium</div>

<!-- Flexbox -->
<div class="flex items-center gap-md">
  <span>Item 1</span>
  <span>Item 2</span>
</div>
```

---

## Accessibility

### Contrast Ratios (WCAG 2.1)
- **Primary Text:** 14.5:1 (AAA)
- **Secondary Text:** 8.2:1 (AAA)
- **Muted Text:** 4.9:1 (AA)
- **Accent Colors:** All meet AA standards

### Focus States
All interactive elements have visible focus states:
```css
input:focus {
  border-color: var(--accent-primary);
  box-shadow: 0 0 0 3px rgba(76, 154, 255, 0.1);
}
```

### Screen Reader Support
- Semantic HTML elements
- ARIA labels where needed
- Keyboard navigation support

### Dark Mode Best Practices
- No pure white text (causes glare)
- No pure black backgrounds (harsh contrast)
- Sufficient spacing between elements
- Clear visual hierarchy

---

## Customization

### Changing Theme Colors

To customize the theme, edit the CSS custom properties in `gui/style.css`:

```css
:root {
  /* Change primary background color */
  --bg-primary: #1a1d2e; /* Darker blue-gray */

  /* Change accent color */
  --accent-primary: #7c3aed; /* Purple accent */

  /* Change text color */
  --text-primary: #f0f0f0; /* Lighter text */
}
```

### Creating a Light Theme

To create a light theme variant (not recommended, but possible):

```css
:root {
  --bg-primary: #ffffff;
  --bg-secondary: #f5f5f5;
  --bg-tertiary: #e0e0e0;
  --text-primary: #1a1a1a;
  --text-secondary: #666666;
  --text-muted: #999999;
}
```

---

## Mobile Optimization

The dark theme is optimized for mobile devices:

### iOS Safari
```html
<!-- Prevents white flash on page load -->
<meta name="theme-color" content="#0f1419">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
```

### Android Chrome
```html
<!-- Dark theme for address bar -->
<meta name="theme-color" content="#0f1419">
<meta name="color-scheme" content="dark">
```

### Responsive Breakpoints
- **Mobile:** < 768px
- **Tablet:** 768px - 992px
- **Desktop:** > 992px
- **Large Desktop:** > 1200px

---

## Performance

### Optimizations
- **CSS Variables:** Single source of truth, no duplicate values
- **Hardware Acceleration:** `transform` and `opacity` for animations
- **Minimal Repaints:** Strategic use of `will-change`
- **Efficient Selectors:** Class-based, low specificity

### File Sizes
- `style.css`: ~20KB (uncompressed)
- `main.css`: ~10KB (uncompressed)
- **Total:** ~30KB CSS (compresses to ~5KB with gzip)

---

## Browser Support

### Fully Supported
- Chrome 90+ ✅
- Firefox 88+ ✅
- Safari 14+ ✅
- Edge 90+ ✅
- Opera 76+ ✅

### Partial Support
- IE 11: CSS variables not supported (fallback to default colors)

### Mobile Browsers
- iOS Safari 14+ ✅
- Chrome Mobile 90+ ✅
- Samsung Internet 14+ ✅

---

## Maintenance

### When Adding New Components
1. Use existing CSS variables
2. Follow existing color patterns
3. Test in light and dark backgrounds
4. Ensure proper contrast ratios
5. Add hover/focus states

### Color Consistency Checklist
- [ ] Uses `var(--bg-*)` for backgrounds
- [ ] Uses `var(--text-*)` for text colors
- [ ] Uses `var(--accent-*)` for interactive elements
- [ ] Has proper hover states
- [ ] Has visible focus states
- [ ] Meets WCAG AA contrast requirements

---

## Troubleshooting

### Text Not Visible
**Issue:** Text appears black on dark background

**Solution:** Ensure you're using CSS variables:
```css
/* ❌ Wrong */
color: black;

/* ✅ Correct */
color: var(--text-primary);
```

### Background Too Bright
**Issue:** Element has light background in dark theme

**Solution:** Use proper background variable:
```css
/* ❌ Wrong */
background-color: white;

/* ✅ Correct */
background-color: var(--bg-secondary);
```

### Focus States Not Visible
**Issue:** Can't see which element is focused

**Solution:** Add explicit focus styles:
```css
button:focus {
  outline: none;
  box-shadow: 0 0 0 3px rgba(76, 154, 255, 0.3);
}
```

---

## Future Enhancements

Potential dark theme improvements:
- [ ] Theme toggle (light/dark/auto)
- [ ] Custom color picker
- [ ] High contrast mode
- [ ] Color blind modes (protanopia, deuteranopia, tritanopia)
- [ ] Accent color presets
- [ ] User theme preferences saved to localStorage

---

## Resources

### Design Inspiration
- GitHub Dark Theme
- VS Code Dark Theme
- Atlassian Design System
- Material Design Dark Theme

### Tools Used
- CSS Custom Properties
- CSS Grid & Flexbox
- CSS Transitions & Animations
- Web Safe Fonts

### References
- [WCAG 2.1 Guidelines](https://www.w3.org/WAI/WCAG21/quickref/)
- [MDN CSS Custom Properties](https://developer.mozilla.org/en-US/docs/Web/CSS/Using_CSS_custom_properties)
- [CSS Tricks - Dark Mode](https://css-tricks.com/a-complete-guide-to-dark-mode-on-the-web/)

---

**The SPINDEX dark theme provides a professional, accessible, and ergonomic user experience for long-term usage.**
