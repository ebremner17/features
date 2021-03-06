<?php

/**
 * Implementation of hook_features_export_options().
 */
function views_features_export_options() {
  $enabled_views = array();

  $views = views_get_all_views();

  foreach ($views as $view) {
    if (!isset($views[$view->name]->disabled) || !$views[$view->name]->disabled) {
      $enabled_views[$view->name] = $view->name;
    }
  }

  ksort($enabled_views);
  return $enabled_views;
}

/**
 * Implementation of hook_features_export_render().
 */
function views_features_export_render($module = 'foo', $data) {
  $code = array();
  $code[] = '/**';
  $code[] = ' * Implementation of hook_views_default_views().';
  $code[] = ' */';
  $code[] = "function {$module}_views_default_views() {";
  $code[] = '  $views = array();';
  $code[] = '';

  // Build views & add to export array
  foreach ($data as $view_name) {
    // Build the view
    $view = views_get_view($view_name);
    if ($view) {
      $code[] = '  // Exported view: '. $view_name;
      $code[] = $view->export('  ');
      $code[] = '  $views[$view->name] = $view;';
      $code[] = '';
    }
  }
  $code[] = '  return $views;';
  $code[] = '}';
  $code = implode("\n", $code);

  return $code;
}

/**
 * Implementation of hook_features_export().
 */
function views_features_export($data, &$export, $module_name = '') {
  if (!isset($export['items']['views'])) {
    $export['items']['views'] = array();
  }

  // Build views & add to export array
  $views = array();
  foreach ($data as $view_name) {
    // Add to exports
    $export['items']['views'][$view_name] = $view_name;

    // Build the view
    $view = views_get_view($view_name);
    if ($view) {
      $view->build();
      $views[$view_name] = $view;
    }
  }

  // Add views as a dependency
  $export['dependencies']['views'] = 'views';

  // Discover module dependencies
  // We need to find dependencies based on:
  // 1. handlers
  // 2. plugins (style plugins, display plugins)
  // 3. other... (e.g. imagecache display option for CCK imagefields) : (

  // Handlers
  $handlers = array('fields', 'filters', 'arguments', 'sort', 'relationships');
  $handler_dependencies = views_handler_dependencies();

  // Plugins
  // For now we only support dependency detection for a subset of all views plugins
  $plugins = array('display', 'style', 'row', 'access');
  $plugin_dependencies = views_plugin_dependencies();

  foreach ($views as $view) {
    foreach ($view->display as $display) {

      // Handlers
      foreach ($handlers as $handler) {
        $items = $display->handler->get_option($handler);
        if (!empty($items)) {
          foreach ($items as $item) {
            $module = $handler_dependencies[$item['table']];
            $export['dependencies'][$module] = $module;
          }
        }
      }

      // Plugins
      foreach ($plugins as $plugin_type) {
        switch ($plugin_type) {
          case 'display':
            $plugin_name = $display->display_plugin;
            break;
          case 'access':
            $access = $display->handler->get_option("access");
            if (!empty($access['type']) && $access['type'] != 'none') {
              $plugin_name = $access['type'];
            }
            else {
              $plugin_name = '';
            }
            break;
          default:
            $plugin_name = $display->handler->get_option("{$plugin_type}_plugin");
            break;
        }
        if (!empty($plugin_name)) {
          $module = $plugin_dependencies[$plugin_type][$plugin_name];
          $export['dependencies'][$module] = $module;
        }
      }
    }
  }
  
  // It's possible that a module provides the view for us so we need to
  // detect this and prevent elusive array_merge_recusive bugs. 
  // To do so we do two things:
  //   1. remove any views in the export that are default views.
  //   2. add as a dependency the module that provides the view.
  //
  // If the view in question is overridden, the user needs to take action
  // to resolve the conflict. We display a message in this situation.
  views_include_default_views();
  $modules = module_implements('views_default_views');
  $conflicts = array();
  foreach ($modules as $m) {
    $callback = $m ."_views_default_views";
    foreach (call_user_func($callback) as $view_name => $view) {
      if (array_key_exists($view_name, $export['items']['views']) && ($m != $module_name)) {
        $v = views_get_view($view_name);
        if ($v) {
          // If view is overridden we display an error message and bail.
          if ($v->type == 'Overridden') {
            $conflicts[$m] = $view_name;
          }
          // If view is default, we add the providing module as a dependency and exclude it from the Views export
          else if ($v->type == 'Default') {
            $tokens = array('!view_name' => $view_name, '!module' => $m);
            $export['conflicts']['status'][] = t('The view <strong>!view_name</strong> will not be exported with your feature because it is provided by <strong>!module</strong> which has been added as a dependency.', $tokens);
            unset($export['items']['views'][$view_name]);
            $export['dependencies'][$m] = $m;
          }
        }
      }
    }
  }
  if ($conflicts) {
    $message = '<p>A conflict was detected with the following overridden view(s):</p>';
    $message .= '<ul><li><strong>Views</strong>: !views</li><li><strong>Providing modules</strong>: !modules</li></ul>';
    $message .= '<p>To resolve this problem you should either:</p>';
    $message .= '<ol><li>Revert each overridden view</li><li>Clone each view so that the modified version(s) can be exported with your feature</li></ol>';
    $tokens = array('!views' => implode(', ', $conflicts), '!modules' => implode(', ', array_keys($conflicts)));
    $export['conflicts']['error'][] = t($message, $tokens);
  }
}

/**
 * Provides an array that maps hook_views_data() tables to modules.
 */
function views_handler_dependencies() {
  static $map;
  if (!isset($map)) {
    $map = array();
    foreach (module_implements('views_data') as $module) {
      $tables = array_keys(module_invoke($module, 'views_data'));
      foreach ($tables as $table) {
        $map[$table] = $module;
      }
    }
  }
  return $map;
}

/**
 * Provides an array that maps hook_views_plugins() to modules.
 */
function views_plugin_dependencies() {
  static $map;
  if (!isset($map)) {
    $map = array();
    foreach (module_implements('views_plugins') as $module) {
      $plugins = module_invoke($module, 'views_plugins');
      foreach ($plugins as $type => $items) {
        if (is_array($items)) {
          foreach (array_keys($items) as $plugin_name) {
            $map[$type][$plugin_name] = $module;
          }
        }
      }
    }
  }
  return $map;
}
