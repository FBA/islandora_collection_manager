<?php
/**
 * @file
 *  a set of utilities for collection management
 */

/**
 * collection creation form
 *
 * @param array $form_state
 * @param string $parent_collection_pid
 * @param string $content_models
 *
 * @return array
 */
function collection_management_form(&$form_state, $this_collection_pid, $content_models) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  module_load_include('inc', 'fedora_repository', 'CollectionPolicy');
  $restricted = FALSE;
  if (variable_get('fedora_namespace_restriction_enforced', TRUE)) {
    $restricted = TRUE;
    $allowed_string = variable_get('fedora_pids_allowed', 'default: demo: changeme: islandora:');
    $namespaces = explode(':', $allowed_string);
    foreach ($namespaces as $namespace) {
      if ($namespace) {
        $allowed[trim($namespace)] = trim($namespace);
      }
    }
  }
  $collection_policy = CollectionPolicy::loadFromCollection($this_collection_pid);

  if ($collection_policy) {
    $supported_collection_models = $collection_policy->getContentModels();
  }
  $collection_namespace = substr($this_collection_pid, 0, strpos($this_collection_pid, ":"));

  $item = new Fedora_Item($this_collection_pid);
  $collection_name = $item->objectProfile->objLabel;
  $new_content_models = get_content_models_as_option_array();
  $current_models_in_policy = array();

  if ($supported_collection_models) {
    foreach ($supported_collection_models as $supported_model) {
      $current_models_in_policy[$supported_model->pid] = $supported_model->pid;
    }
  }
  $cm_options = array();
  $name_mappings = array();
  foreach ($content_models as $content_model) {
    if ($content_model->pid != "islandora:collectionCModel") {
      $item = new fedora_item($content_model->pid);
      $cm_name = $item->objectProfile->objLabel;
      $cm_options[$content_model->pid] = $cm_name;
    }
  }

  if (!empty($current_models_in_policy)) {
    $show_delete = TRUE;
  }


  $content_models = get_content_models_as_option_array();
  unset($content_models['islandora:collectionCModel']);

  $form['child_creation'] = array(
    '#title' => "Create Child Collection",
    '#type' => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );

  $form['child_creation']['titlebox'] = array(
    '#type' => 'item',
    '#value' => t("Create New Child Collection within $this_collection_pid"),
  );

  $form['child_creation']['collection_name'] = array(
    '#title' => "Collection Name",
    '#type' => 'textfield',
    '#description' => t("Human readable name for this collection"),
  );

  $form['child_creation']['new_collection_pid'] = array(
    '#title' => "Collection PID",
    '#type' => 'textfield',
    '#size' => 15,
    '#default_value' => Fedora_Item::get_next_PID_in_namespace($collection_namespace),
    '#description' => t("Unique PID for this collection.  <br />Pids take the general form of namespace:collection eg islandora:pamphlets"),
  );
  if (!$restricted) {
    $form['child_creation']['collection_namespace'] = array(
      '#title' => "Collection Namespace",
      '#type' => 'textfield',
      '#size' => 15,
      '#default_value' => $collection_namespace,
      '#description' => t("Namespace for objects in this collection."),
    );
  }
  else {
    $form['child_creation']['collection_namespace'] = array(
      '#title' => "Collection Namespace",
      '#type' => 'select',
      '#options' => $allowed,
      '#default_value' => 'default',
      '#description' => t("Namespace for objects in this collection."),
    );
  }
  $form['parent_collection'] = array(
    '#type' => 'hidden',
    '#value' => $this_collection_pid,
  );

  $form['collection_pid'] = array(
    '#type' => 'hidden',
    '#value' => $this_collection_pid,
  );
  $form['child_creation']['content_models'] = array(
    '#title' => "Choose allowable content models for this collection",
    '#type' => 'checkboxes',
    '#options' => $content_models,
    '#description' => t("Content models describe the behaviours of objects with which they are associated."),
  );


  $form['child_creation']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create Collection'),
    '#id' => 'create_child',
  );

  return($form);
}

/**
 * collection creation form validate
 *
 * @param array $form
 *   The form being validated.
 * @param array $form_state
 *   The state of the form being validate.
 */
function collection_management_form_validate($form, &$form_state) {
  if ($form_state['clicked_button']['#id'] == 'create_child') {
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
    $pid = $form_state['values']['new_collection_pid'];
    $item = new fedora_item($pid);
    $needs_model = FALSE;
    foreach ($form_state['values']['content_models'] as $key => $value) {
      if (is_string($value)) {

        $needs_model = FALSE;
      }
    }
    $checked = array_values($form_state['values']['content_models']);
    if ($item->exists()) {
      form_set_error('new_collection_pid', t("$pid already exists within your repository. the PID must be unique. Click on 'Manage This Collection' tab and enter new value."));
      return;
    }
    if (!valid_pid($pid)) {
      form_set_error('new_collection_pid', t("$pid is not a valid identifier. Click on 'Manage This Collection' tab and enter new value."));
      return;
    }
    if ($needs_model) {
      form_set_error('content_models', t("At least one content model must be selected. Click on 'Manage This Collection' tab and enter content model."));
      return;
    }
  }
  if ($form_state['clicked_button']['#id'] == 'add_cm') {

    $name_parts = explode(":", $form_state['values']['new_cp_namespace']);
    $namespace = $name_parts[0] . ":1";
    if (!valid_pid($namespace)) {
      form_set_error('new_cp_namespace', t("Namespace must be valid. Click on 'Manage This Collection' tab and enter new value."));

      return;
    }
    $form_state['values']['new_cp_namespace'] = $namespace;
  }
}

/**
 * collection creation form submit
 *
 * @global user $user
 * @param arary $form
 * @param array $form_state
 */
function collection_management_form_submit($form, &$form_state) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/dublin_core');
  global $user;
  $collection_pid = $form_state['values']['parent_collection'];
  $policy = CollectionPolicy::loadFromCollection($collection_pid, TRUE);
  $collection_policy = '<?xml version="1.0" encoding="UTF-8"?>
<collection_policy xmlns="http://www.islandora.ca" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" name="" xsi:schemaLocation="http://www.islandora.ca http://syn.lib.umanitoba.ca/collection_policy.xsd">
    <content_models>
    <content_model dsid="ISLANDORACM" name="Islandora Collection Model ~ islandora:collectionCModel" namespace="islandora:1" pid="islandora:collectionCModel"/>
    </content_models>
    <search_terms>
    </search_terms>
    <staging_area></staging_area>
    <relationship>isMemberOfCollection</relationship>
</collection_policy>';


  // Add child collection to policy.
  if ($form_state['clicked_button']['#id'] == 'create_child') {
    $module_path = drupal_get_path('module', 'fedora_repository');
    $thumbnail = drupal_get_path('module', 'Fedora_Repository') . '/images/Crystal_Clear_filesystem_folder_grey.png';
    $new_collection_pid = $form_state['values']['new_collection_pid'];
    $new_collection_label = $form_state['values']['collection_name'];
    $pid_namespace = $form_state['values']['collection_namespace'];
    $all_cModels = get_content_models_as_option_array();

    $content_models = $form_state['values']['content_models'];
    $collection_policy_xml = simplexml_load_string($collection_policy);
    foreach ($content_models as $content_model) {
      if ($content_model) {
        $node = $collection_policy_xml->content_models->addChild('content_model');
        $node->addAttribute('dsid', 'ISLANDORACM');
        $node->addAttribute('name', $all_cModels[$content_model]);
        $node->addAttribute('namespace', $pid_namespace . ':1');
        $node->addAttribute('pid', $content_model);
      }
    }
    $item = fedora_item::ingest_new_item($new_collection_pid, 'A', $new_collection_label, $user->name);
    $item->add_relationship('isMemberOfCollection', $collection_pid, RELS_EXT_URI);
    $item->add_relationship('hasModel', 'islandora:collectionCModel', FEDORA_MODEL_URI);
    $item->add_datastream_from_string($collection_policy_xml->saveXML(), 'COLLECTION_POLICY', 'COLLECTION_POLICY', 'text/xml', 'X');
    $item->add_datastream_from_file($thumbnail, 'TN');
    drupal_goto("/fedora/repository/$new_collection_pid");
  }


  // Add content model to collection policy.
  if ($form_state['clicked_button']['#id'] == 'add_cm') {
    if (!$policy) {
      $item = new Fedora_Item($collection_pid);
      $item->add_datastream_from_string($collection_policy, 'COLLECTION_POLICY', 'COLLECTION_POLICY', 'text/xml', 'X');
      $policy = CollectionPolicy::loadFromCollection($collection_pid, TRUE);
    }

    $cp_namespace = $form_state['values']['new_cp_namespace'];
    $cp_content_model = $form_state['values']['content_model_to_add'];
    $policy->addModel(ContentModel::loadFromModel($cp_content_model), $cp_namespace);
    $policy->saveToFedora();
    drupal_set_message("Collection model successfully added");
  }

  // Remove content model from collection policy.
  if ($form_state['clicked_button']['#id'] == 'remove_cm') {
    $candidates = $form_state['values']['content_models_to_remove'];
    $count = 0;
    foreach ($candidates as $candidate) {
      if (is_string($candidate)) {
        $policy->removeModel(ContentModel::loadFromModel($candidate));
        $count++;
      }
    }
    if ($count > 0) {
      $policy->saveToFedora();
      if ($count > 1) {
        $s = 's';
      }
      drupal_set_message("$count collection model$s removed");
    }
  }


  // Change content model on selected objects.
  if ($form_state['clicked_button']['#id'] == 'change_model') {
    $current_content_model = $form_state['values']['current_content_model'];
    $new_content_model = $form_state['values']['new_content_model'];

    $add_to_policy = TRUE;
    $policy_cms = $policy->getContentModels();
    foreach ($policy_cms as $policy_cm) {
      if ($policy_cm->pid == $current_content_model) {
        $namespace = $policy_cm->pid_namespace;
      }
      if ($policy_cm->pid == $new_content_model) {
        $add_to_policy = FALSE;
      }
    }
    if ($add_to_policy) {
      $policy->addModel(ContentModel::loadFromModel($new_content_model), $namespace);
      $policy->saveToFedora();
    }
    $query = "select \$object  from <#ri>
    where (\$object <info:fedora/fedora-system:def/model#hasModel> <info:fedora/$current_content_model>
      and (\$object <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> <info:fedora/$collection_pid>
           or \$object <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/$collection_pid>)
      and \$object <fedora-model:state> <info:fedora/fedora-system:def/model#Active>)";
    $query = htmlentities(urlencode($query));
    $content = '';

    $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
    $url .= "?type=tuples&flush=TRUE&format=csv&limit=$limit&offset=$offset&lang=itql&stream=on&query=" . $query;
    $content .= do_curl($url);
    $results = explode("\n", $content);
    $object_pids = preg_replace('/^info:fedora\/|"object"/', '', $results);
    $count = 0;
    foreach ($object_pids as $object_pid) {
      if (!$object_pid) {
        continue;
      }
      $item = new fedora_item($object_pid);
      $item->purge_relationship('hasModel', $current_content_model);
      $item->add_relationship('hasModel', $new_content_model, FEDORA_MODEL_URI);
      $count++;
    }
    drupal_set_message("$current_content_model changed to $new_content_model on $count objects");
  }

}

/**
 * deletes PID
 *
 * @param string $pid
 */
function delete_objects_as_batch($pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $name = $user->name;
  $item_to_delete = new Fedora_Item($pid);
  $item_to_delete->purge("$object purged by $name");
}

/**
 * removes association of this object to this collection
 *
 * @param string $pid
 * @param string $collection_pid
 */
function remove_collection_association($pid, $collection_pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $item = new Fedora_Item($pid);
  $item->purge_relationship('isMemberOfCollection', $collection_pid);
}

/**
 *
 * @param unknown_type $collection_pid
 */
function get_child_collections($collection_pid) {

  $query = <<<EOD
  select \$object from <#ri>
    where \$object <info:fedora/fedora-system:def/model#hasModel> <info:fedora/islandora:collectionCModel>
    and \$object  <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> <info:fedora/$collection_pid>
EOD;

  $query = htmlentities(urlencode($query));
  $content = '';

  $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  $url .= "?type=tuples&flush=TRUE&format=csv&limit=$limit&offset=$offset&lang=itql&stream=on&query=" . $query;
  $content .= do_curl($url);
  $results = explode("\n", $content);
  $lines = preg_replace('/info:fedora\/|"object"/', '', $results);
  $collection_pids = array_values(array_filter($lines));
  return $collection_pids;
}
