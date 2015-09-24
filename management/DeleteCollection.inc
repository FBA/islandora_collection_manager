<?php

/**
 * @file
 *   Holds the code related to deleting objects/collections for this module.
 */

/**
 * The form for deleting collections and their members.
 *
 * @param array $form_state
 *   The state of the form
 * @param string $pid
 *   The collection PID
 *
 * @return array
 *   The completed form array.
 */
function islandora_collection_deletion_form(&$form_state, $pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  module_load_include('inc', 'islandora_collection_manager', 'CollectionManagerTable');
  module_load_include('inc', 'islandora_collection_manager', 'CollectionManagement');
  $potential_collections = get_collections_as_option_array();
  $table = islandora_collection_table($pid);
  $deletion_message = ($table) ? "Delete Members of this Collection" : "Delete Collection";
  $submit_text_message = ($table) ? "Delete Selected Objects" : "Delete Collection";
  $path = drupal_get_path('module', 'islandora_collection_manager');
  drupal_add_js($path . '/js/collection.js?stopcache');
  drupal_add_css($path . '/css/collection.css');

  $form = array();

  $form['link'] = array(
    '#type' => 'item',
    '#value' => l(t("Return to collection view"), "fedora/repository/$pid"),
  );

  $form['collection_delete'] = array(
    '#title' => $deletion_message,
    '#type' => 'fieldset',
    '#description' => t("if a selected item is a member of more than one collection, the relationship with this collection will be severed.<br />
      If a selected item is a member of only this collection it will be <strong>permanently deleted.</strong>")
  );

  if ($table) {
    $form['collection_delete']['table'] = array(
      'table' => $table,
    );
  }
  else {
    $form['collection_delete']['delete_root'] = array(
      '#type' => 'checkbox',
      '#title' => "Remove this empty collection?",
      '#id' => 'delete_collection',
    );
  }
  $form['current'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );

  $form['collection_delete']['message'] = array(
    '#type' => 'item',
    '#value' => t("This action is permanant and cannot be undone."),
  );
  $form['collection_delete']['submit'] = array(
    '#type' => 'submit',
    '#description' => t("This action is permanant and cannot be undone."),
    '#value' => t($submit_text_message),
  );

  if (user_access('delete entire collections') && user_access('purge objects and datastreams')) {
    return $form;
  }
}

/**
 * The submit hook for the deletion form. Actualy deletes objects
 *
 * @param array $form
 *   The form to handle.
 * @param array $form_state
 *   The state of the form to handle
 */
function islandora_collection_deletion_form_submit($form, &$form_state) {
  global $user;
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');

  $collection_pid = $form_state['values']['current'];
  $parents = get_parent_collections_from_pid($collection_pid);

  if ($form_state['values']['delete_root'] == 1) {
    delete_root_collection($collection_pid);
    drupal_goto("fedora/repository/" . $parents[0]);
  }

  $child_collections = get_child_collections($collection_pid);
  $current = $form_state['values']['current'];
  $populated_child_collections = array();
  $pids = @array_filter($form_state['values']['table']['selections']);

  if (!empty($child_collections)) {
    foreach ($child_collections as $child) {
      $child_pids = get_related_items_as_array($child, 'isMemberOfCollection');
      if (!empty($child_pids)) {
        $populated_child_collections[] = $child;
      }
    }
  }
  if (!empty($populated_child_collections)) {
    $conflict = FALSE;
    foreach ($populated_child_collections as $collection) {
      if (in_array($collection, $pids)) {
        $conflict = TRUE;
        drupal_set_message("Populated child collections were not deleted.");
      }
    }
  }
  $pids_to_delete = array_diff($pids, $populated_child_collections);
  $batch = array(
    'title' => "Deleting Selected Objects from collection",
    'progress_message' => t('Deleted @current objects out of @total.'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'islandora_collection_manager') . '/management/DeleteCollection.inc',
  );

 /**
  *
  * FBA MODIFICATION BY NBYWELL on 20/10/2014 to process any link-data
  * that was created by the Object Linker functionality. This could
  * be RDFs and entries in the datastreams of linked-to objects.
  * The higher level function process_links_on_purge is not called
  * in this function because of the requirement to only load the
  * XML-Form when necessary, so checking is carried out before the
  * lower level function remove_links_on_purge is called. The $batch
  * array is passed to the process_links_on_purge function because
  * it may add operations to the batch queue.
  */
  module_load_include('inc', 'islandora_content_model_forms', 'ObjectLinker');
  if (variable_get('rdf_relationships', NULL)) {
    // An unrelated rdf_relationships variable is
    // present. This happens if the user loaded
    // up an XML-form but never clicked submit.
    variable_set('rdf_relationships', NULL);
  }

  $content_model_pid_current = '';
  $content_model_pid_previous = '';
  $content_model_and_form_combination_previous_valid = false;
  $pids_to_delete_count = 0;
  
  foreach ($pids_to_delete as $pid_to_delete) {
    // Avoid unnecessary XML-form building by determining
    // whether the content model (and therefore the form) for
    // the previous PID is the same as for the current PID.
    // Note that for object linking to work with regard to
    // purging, each content model should only have one
    // associated form. If this is not the case, the purge
    // will go ahead, but any links created by the Object
    // Linker will not be unwound.
    $form_build_required = true;
    $content_model_form_association_valid = true;
    $content_model_pid_current = get_object_linker_content_model($pid_to_delete);
    if (strlen($content_model_pid_current) > 0) {
      if (!strcmp($content_model_pid_current, $content_model_pid_previous)) {
        if ($content_model_and_form_combination_previous_valid) {
          $form_build_required = false;
        } else {
          $content_model_form_association_valid = false;
        }
      }
    } else {
      $content_model_form_association_valid = false;
    }

    if ($form_build_required) {
      $object_linker_form_name = get_object_linker_form_name($pid_to_delete, $content_model_pid_current);
      if (strlen($object_linker_form_name) > 0) {
        build_object_linker_form($object_linker_form_name);
        $content_model_and_form_combination_previous_valid = true; 
      } else {
        $content_model_form_association_valid = false;
      }
    }
    if ($content_model_form_association_valid) {
      if (variable_get('rdf_relationships', NULL)) {
        remove_links_on_purge($batch, $pid_to_delete);
      }
    } else {
      $content_model_and_form_combination_previous_valid = false; 
    }

    $content_model_pid_previous = $content_model_pid_current;

    $batch['operations'][] = array('collection_manager_delete', array($pid_to_delete, $collection_pid));
    $pids_to_delete_count += 1;
  }

  if (variable_get('rdf_relationships', NULL)) {
    variable_set('rdf_relationships', NULL);
  }

  batch_set($batch);
}

/**
 * This function will delete a collection.
 *
 * @param string $pid
 *   The PID of the item to purge.
 */
function delete_root_collection($pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $item = new Fedora_Item($pid);
  $item->purge();
  drupal_set_message("$pid deleted.");
}

/**
 *  This function will delete an object.
 *
 * @param string $pid
 *   The PID of the item to purge.
 */
function collection_manager_delete($pid, $collection_pid) {
  global $user;
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $item = new Fedora_Item($pid);
  $relationships = $item->get_rdf_relationships();
  $membership_count = count($relationships['fedora:isMemberOfCollection']);
  if ($membership_count == 1) {
    $item->purge("Purged by " . $user->name);
  }
  else {
    $item->purge_relationship('isMemberOfCollection', $collection_pid);
  }
  $item->forget();
}
