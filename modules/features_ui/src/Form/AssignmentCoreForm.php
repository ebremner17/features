<?php

/**
 * @file
 * Contains \Drupal\features_ui\Form\AssignmentCoreForm.
 */

namespace Drupal\features_ui\Form;

use Drupal\features_ui\Form\AssignmentFormBase;
use Drupal\Core\Form\FormStateInterface;

/**
 * Configures the selected configuration assignment method for this site.
 */
class AssignmentCoreForm extends AssignmentFormBase {

  /**
   * {@inheritdoc}
   */
  public function getFormID() {
    return 'features_assignment_core_form';
  }

  /**
   * {@inheritdoc}
   */
  public function buildForm(array $form, FormStateInterface $form_state) {

    $defaults = $this->configFactory->get('features.assignment')->get('core.types');
    $this->setTypeSelect($form, $defaults, $this->t('core'));
    $this->setActions($form);

    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function submitForm(array &$form, FormStateInterface $form_state) {
    $types = array_filter($form_state->getValue('types'));

    $this->configFactory->getEditable('features.assignment')->set('core.types', $types)->save();

    $form_state->setRedirect('features.assignment');
    drupal_set_message($this->t('Package assignment configuration saved.'));
  }

}