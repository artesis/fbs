<?php
/**
 * @file
 * Implements functions used to log users into the system through WAYF.
 */

/**
 * Implements hook_wayf_login_credentials().
 *
 * Used to set the name and password to log into the library system after WAYF
 * login.
 */
function fbs_wayf_login_credentials($authentication_attributes) {
  // Retrieve the CPR number from the UCFMD service.
  if (module_exists('ding_ucfmd')) {
    $cpr = module_invoke(
      'ding_ucfmd',
      'retrieve_cpr',
      $authentication_attributes['organizationName'],
      $authentication_attributes['eduPersonPrincipalName']
    );
  }

  $pass = user_password(16);
  _fbs_wayf_login($authentication_attributes['eduPersonPrincipalName'], $pass);
  $credentials = array(
    'name' => $cpr,
    'pass' => $pass,
  );

  return $credentials;
}

/**
 * Implements hook_wayf_attributes().
 *
 * Used to define which attribute to use as login name.
 */
function fbs_wayf_attributes() {
  return array(
    'organizationName' => array(
      'field' => NULL,
    ),
    'eduPersonPrincipalName' => array(
      'field' => NULL,
    ),
  );
}
