<?php

/**
 * @file
 * Handles user loans.
 */

/**
 * Fetches user loans.
 *
 * @param $account
 *   User object whose loans are fetched.
 *
 * @return array
 *   Set of user loans.
 */
function fbs_loan_raw_list(stdClass $account) {
  $fbs_loans_result = &drupal_static(__FUNCTION__ . $account->uid, array());

  $service = fbs_service();
  if ($service && empty($fbs_loans_result)) {
    try {
      $fbs_loans_result = $service->MaterialLoans->getLoans($service->agencyId, fbs_patron_id($account));
    }
    catch (Exception $e) {
      watchdog_exception('fbs', $e);
    }
  }

  return $fbs_loans_result;
}

/**
 * Implements provider loan, list.
 *
 * @param object $account
 *   User to fetch list for.
 *
 * @return array
 *   Loan information.
 */
function fbs_loan_list($account) {
  $result = array();
  $fbs_loans_result = fbs_loan_raw_list($account);

  // Lookup v3 loan id's.
  $loan_ids = array();
  foreach ($fbs_loans_result as $loan) {
    $loan_ids[] = $loan->loanDetails->recordId;
  }
  $loan_ids = ding_provider_lookup_entity_id($loan_ids);

  $result = array();
  foreach ($fbs_loans_result as $loan) {
    $loan_data = array(
      'ding_entity_id' => $loan_ids[$loan->loanDetails->recordId],
      'loan_date' => $loan->loanDetails->loanDate,
      'expiry' => $loan->loanDetails->dueDate,
      'renewable' => $loan->isRenewable,
      'materials_number' => $loan->loanDetails->materialItemNumber,
    );

    // If this is a periodical, add in issue data.
    if (isset($loan->loanDetails->periodical)) {
      $periodical = $loan->loanDetails->periodical;
      $vol = $periodical->volume;
      if (!empty($periodical->volumeNumber)) {
        $vol .= '.' . $periodical->volumeNumber;
      }
      if (!empty($periodical->volumeYear)) {
        $loan_data['notes'] = t('Issue @vol, @year', array('@vol' => $vol, '@year' => $periodical->volumeYear));
      }
      else {
        $loan_data['notes'] = t('Issue @vol', array('@vol' => $vol));
      }
    }

    // Handle inter library loans.
    if (!empty($loan->loanDetails->ilBibliographicRecord)) {
      $loan_data['display_name'] = $loan->loanDetails->ilBibliographicRecord->title;
    }

    $id = $loan->loanDetails->loanId;
    $result[$id] = new DingProviderLoan($id, $loan_data);
  }

  return $result;
}

/**
 * Implements provider loan, renew.
 *
 * @param object $account
 *   User to renew loan for.
 * @param array $loan
 *   Loan ids to renew.
 *
 * @return array
 *   Result of revewals.
 */
function fbs_loan_renew($account, $loans) {
  $res = array();
  try {
    $res = fbs_service()->MaterialLoans->renewLoans(fbs_service()->agencyId, fbs_patron_id($account), $loans);
  }
  catch (Exception $e) {
    watchdog_exception('fbs', $e);
  }

  $result = array();
  foreach ($res as $loan) {
    // Using an array for renewalStatus is an odd choice, but we'll only
    // consider the loan renewed if 'renewed' is the only status.
    if ($loan->renewalStatus == array('renewed')) {
      $result[$loan->loanDetails->loanId] =
        $loan->loanDetails->loanType == 'interLibraryLoan' ?
        DingProviderLoan::STATUS_RENEWAL_REQUESTED :
        DingProviderLoan::STATUS_RENEWED;
    }
    else {
      $result[$loan->loanDetails->loanId] = DingProviderLoan::STATUS_NOT_RENEWED;
      if (in_array('deniedMaxRenewalsReached', $loan->renewalStatus)) {
        $result[$loan->loanDetails->loanId] = DingProviderLoan::STATUS_RENEWAL_NUM_EXCEEDED;
      }
      if (in_array('deniedReserved', $loan->renewalStatus)) {
        $result[$loan->loanDetails->loanId] = DingProviderLoan::STATUS_RENEWAL_RESERVED;
      }
    }
  }

  return $result;
}
