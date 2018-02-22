<?php

/**
 * @file
 * Handles user debts.
 */

/**
 * Fetches user debts.
 *
 * @param $account
 *   User object whose debts are fetched.
 *
 * @return array
 *   Set of user debts.
 */
function fbs_debt_raw_list(stdClass $account) {
  $fbs_debts_result = &drupal_static(__FUNCTION__ . $account->uid, array());

  $service = fbs_service();
  if ($service && empty($fbs_debts_result)) {
    try {
      $fbs_debts_result = fbs_service()->Payment->getFees(fbs_service()->agencyId, fbs_patron_id($account), FALSE, FALSE);
    }
    catch (Exception $e) {
      watchdog_exception('fbs', $e);
    }
  }

  return $fbs_debts_result;
}

/**
 * Implements provider debt, list.
 *
 * @param object $account
 *   User to get debt listing for.
 *
 * @return array
 *   Debt information.
 */
function fbs_debt_list($account) {
  $result = array();
  $fbs_debts_result = fbs_debt_raw_list($account);

  $debt_ids = array();
  foreach ($fbs_debts_result as $fee) {
    // The entity is only fetched in this case.
    if (!empty($fee->materials) && count($fee->materials) == 1) {
      $material = reset($fee->materials);
      $debt_ids[$material->recordId] = $material->recordId;
    }
  }
  $debt_ids = ding_provider_lookup_entity_id($debt_ids);

  foreach ($fbs_debts_result as $fee) {
    $id = $fee->feeId;

    $data = array(
      'date' => $fee->creationDate,
      'display_name' => $fee->reasonMessage,
      // FBS only provides the amount left to be paid on each fee, so this
      // will only reflect that.
      'amount' => $fee->amount,
      // And the only thing we can do here is set it to zero, as the original
      // amount isn't available.
      'amount_paid' => 0,
      'invoice_number' => NULL,
      'type' => $fee->reasonMessage,
    );

    if (!empty($fee->materials)) {
      $record_ids = array();
      $material_ids = array();
      foreach ($fee->materials as $material) {
        $material_ids[] = $material->materialItemNumber;
        $record_ids[] = $material->recordId;
      }

      // If there's only one material, use its title as display_name.
      if (count($record_ids) == 1) {
        $record_id = reset($record_ids);
        $entity = ding_entity_load($debt_ids[$record_id]);
        if (!empty($entity)) {
          $data['display_name'] = $entity->getTitle();
        }
      }
      elseif (count($record_ids) > 1) {
        $data['display_name'] = t('Multiple materials');
      }
      $data['material_number'] = implode(', ', $material_ids);
    }

    $result[$id] = new DingProviderDebt($id, $data);

  }

  return $result;
}

/**
 * Implements provider debt, payment_received.
 *
 * @param object $account
 *   User to register payment for.
 * @param array $debt_ids
 *   Depts paid for.
 * @param string $order_id
 *   Transaction id.
 *
 * @return bool
 *   Success or not.
 */
function fbs_debt_payment_received($account, $debt_ids = array(), $order_id = NULL) {
  $payment_order = new FBS\Model\PaymentOrder();
  $payment_order->orderId = $order_id;
  $payment_order->feeIds = array();

  foreach ($debt_ids as $debt_id) {
    $payment_order->feeIds[] = $debt_id;
  }

  $res = array();
  try {
    $res = fbs_service()->Payment->payFees(fbs_service()->agencyId, fbs_patron_id($account), $payment_order);
  }
  catch (Exception $e) {
    watchdog_exception('fbs', $e);
  }

  foreach ($res as $confirmation) {
    if ($confirmation->paymentStatus !== 'paymentRegistered') {
      $variables = array(
        '@patron_id' => fbs_patron_id($account),
        '@order_id' => $order_id,
        '@fee_id' => implode(', ', $debt_ids),
      );
      watchdog('fbs', 'Could not register payment for patron @patron_id, order id @order_id, fee id @fee_id', $variables, WATCHDOG_ERROR);
      return FALSE;
    }
  }

  return TRUE;
}
